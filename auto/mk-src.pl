#!/usr/bin/perl

#
# mk-src.pl -- Perl program to automatically generate interface code.
#
# This file is part of The New Aspell
# Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
# license version 2.0 or 2.1.  You should have received a copy of the
# LGPL license along with this library if you did not you can find it
# at http://www.gnu.org/.

######################################################################
#
# Prologue
#

use strict;
use warnings;
no warnings qw(uninitialized);
no locale;

use Data::Dumper;

sub false () {return 0}
sub true () {return 1}

sub to_upper( $ );
sub to_lower( $ );
sub to_mixed( $ );

sub creates_type ( $ );
sub update_type ( $ ; $ );
sub finalized_type ( $ );
sub need_options ( $ );
sub valid_option ( $ $ );
sub valid_group ( $ $ );
sub store_group ( $ $ );
sub copy_methods ( $ $ $ );
sub create_file ( $ $ );

sub cmap ( & @ ) {
  my ($sub, @d) = @_;
  return join '', map &$sub, @d;
}

sub one_of ( $ @ ) {
  my ($v, @l) = @_;
  return scalar grep {$_ eq $v} @l;
}

my %info;

my %types;
my %methods;

my $master_data = {type=>'root', name=>undef};

my $line = '';
my $level = 0;
my $base_level = 0;

######################################################################
#
# Setup variables
#

INIT {
  $Data::Dumper::Indent = 1;
  #$Data::Dumper::Purity = 1;
  #$Data::Dumper::Terse = 1;
  $Data::Dumper::Quotekeys = 0;

  %info =
    (
     root => {options => [],
	      groups => ['methods', 'group']},
     methods => {options => ['strip', 'prefix', 'c impl headers'],
		 groups => undef},
     group => {options => ['no native'],
	       groups => ['enum', 'struct', 'union', 'func', 'class', 'errors']},
     enum => {options => ['desc', 'prefix'],
	      creates_type => 'enum'},
     struct => {options => ['desc', 'treat as object'],
		 groups => undef,
		 creates_type => 'struct',},
     union => {options => ['desc', 'treat as object'],
	       groups => undef,
	       creates_type => 'union'},
     class => {options => ['c impl headers'],
	       groups => undef,
	       creates_type => 'class'},
     errors => {},
     method => {options => ['desc', 'posib err', 'c func', 'const',
			    'c only', 'c impl', 'cxx impl'],
		groups => undef},
     constructor => {options => ['returns alt type', 'c impl', 'desc'],
		     groups => 'types'},
     destructor => {options => [],
		    groups => undef},
    );

  foreach my $t ('void',
		 'string', 'encoded string', 'string obj',
		 'bool', 'pointer',
		 'double',
		 'char', 'unsigned char',
		 'short', 'unsigned short',
		 'int', 'unsigned int',
		 'long', 'unsigned long') {
    update_type $t, {type=>'basic'};
  }
}


######################################################################
#
# Parse
#

sub advance ( );
sub advance ( ) {
  $line = undef;
  do {
    $line = <IN>;
    return unless defined $line;
    $line =~ s/\#.*$//;
    $line =~ s/^(\t*)//;
    $level = $base_level + length($1);
    $line =~ s/\s*$//;
    ++$base_level if $line =~ s/^\{$//;
    --$base_level if $line =~ s/^\}$//;
    $line =~ s/\\([{}])/$1/g;
  } while ($line eq '');
  #print "$level:$line\n";
}

sub parse ( $ $ );
sub parse ( $ $ ) {
  my ($data, $this_level) = @_;
  if (need_options $data) {
    for (;;) {
      return          if $level < $this_level;
      return          unless defined $line;
      die             if $level > $this_level;
      last            if $line eq '/';
      my $k;
      ($k, $line) = split /\=\>/, $line;
      $k =~ s/^\s*(.+?)\s*$/$1/;
      my $v = $line;
      print STDERR "The option \"$k\" is invalid for the group \"$data->{type}\"\n"
	unless valid_option $data, $k;
      advance;
      for (;;) {
	return unless defined $line;
	last if $level <= $this_level;
	$v .= "\n$line";
	advance;
      }
      $v =~ s/^[ \t\n]+//;
      $v =~ s/[ \t\n]+$//;
      $data->{$k} = $v;
    }
    return unless $line eq '/';
    advance;
  } else {
    advance if $line eq '/';
  }
  $data->{data} = [];
  for (;;) {
    return if $level < $this_level;
    return unless defined $line;
    die    if $level > $this_level;
    my ($type, $name) = split /:/, $line;
    $type =~ s/^\s*(.+?)\s*$/$1/;
    $name =~ s/^\s*(.+?)\s*$/$1/;
    print STDERR "The subgroup \"$type\" is invalid in the group \"$data->{type}\"\n"
      unless valid_group $data, $type;
    my $d = {type=>$type, name=>$name};
    store_group $d, $data;
    advance;
    parse($d, $this_level + 1);
  }
}

open IN, "auto/mk-src.in" or die;
advance;
parse $master_data, 0;
close IN;

######################################################################
#
# Prep
#

sub prep ( $ $ $ );
sub prep ( $ $ $ ) {
  my ($data, $group, $stack) = @_;
  my $d = creates_type $data;
  update_type $d->{name}, {%$d,created_in=>$group} if (defined $d);
  $group = $data->{name} if $data->{type} eq 'group';
  $stack = {%$stack, prev=>$data, $data->{type}=>$data};
  if ($data->{type} eq 'method') {
    die unless $data->{data};
    $data->{data}[0]{'posib err'} = true if exists $data->{'posib err'};
  }
  my $i = 0;
  my $lst = $data->{data};
  return unless defined $lst;
  while ($i != @$lst) {
    my $d = $lst->[$i];
    if (exists $methods{$d->{type}}) {
      splice @$lst, $i, 1, copy_methods($d, $data, $stack->{class}{name});
    } else {
      prep $d, $group, $stack;
      ++$i;
    }
  }
}

prep $master_data, '', {};

######################################################################
#
# Create C Interface
#

sub create_cc_file ( % )  {
  my (%p) = @_;
  $p{name} = $p{data}{name} unless exists $p{name};
  $p{ext} = $p{cxx} ? ($p{header} ? 'hpp' : 'cpp') : 'h';
  my $body;
  my %accum = exists $p{accum} ? (%{$p{accum}}) : ();
  foreach my $d (@{$p{data}{data}}) {
    next unless exists $info{$d->{type}}{proc}{$p{type}};
    $body .= $info{$d->{type}}{proc}{$p{type}}->($d, \%accum);
  }
  return unless length($body) > 0;
  my $file = <<'---';
/* Automatically generated file.  Do not edit directly. */

/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

---
  my $hm = "ASPELL_". to_upper($p{name})."__".to_upper($p{ext});
  $file .= "#ifndef $hm\n#define $hm\n\n" if $p{header};
  $file .= cmap {"#include \"".to_lower($_).".hpp\"\n"} sort keys %{$accum{headers}};
  $file .= "#ifdef __cplusplus\nextern \"C\" {\n#endif\n" if $p{header} && !$p{cxx};
  $file .= "\nnamespace acommon {\n\n" if $p{cxx};
  $file .= cmap {"$_->{type} ".to_mixed($_->{name}).";\n"}
                (sort {$a->{name} cmp $b->{name}} values %{$accum{types}})
                                                                    if $p{cxx};
  $file .= "\n";
  $file .= $body;
  $file .= "\n\n}\n\n" if $p{cxx};
  $file .= "#ifdef __cplusplus\n}\n#endif\n" if $p{header} && !$p{cxx};
  $file .= "#endif /* $hm */\n" if $p{header};
  create_file $p{dir}.'/'.to_lower($p{name}).$p{pre_ext}.'.'.$p{ext}, $file;
}

#
# Pure C
#

create_cc_file (type => 'cc',
		dir => 'interfaces/cc',
		name => 'aspell',
		header => true,
		data => $master_data);

#
# Native
#

foreach my $d (@{$master_data->{data}}) {
  $info{group}{proc}{native}->($d);
}

#
# C for C++
#

foreach my $d (@{$master_data->{data}}) {
  $info{group}{proc}{impl}->($d);
}

#
# Impl
#

foreach my $d (@{$master_data->{data}}) {
  $info{group}{proc}{native_impl}->($d);
}

############################################################

INIT {
  sub to_c_return_type ( $ ) {
    my ($d) = @_;
    return $d->{type} unless exists $d->{'posib err'};
    return 'int' if one_of $d->{type}, ('void', 'bool', 'unsigned int');
    return $d->{type};
  }

  sub c_error_cond ( $ ) {
    my ($d) = @_;
    die unless exists $d->{'posib err'};
    return '-1' if one_of $d->{type}, ('bool', 'unsigned int', 'int');
    return '0';
  }

  sub to_type_name ( $ $ ; \% );
  sub to_type_name ( $ $ ; \% ) {
    my ($d, $p, $accum) = @_;
    $accum = {} unless defined $accum;

    my $mode = $p->{mode};
    die unless one_of $mode, qw(cc cc_cxx  native native_no_err  cxx);
    my $is_cc = one_of $mode, qw(cc cc_cxx);
    my $is_native = one_of $mode, qw(native native_no_err);

    my $pos  = $p->{pos};
    my $t = finalized_type($pos eq 'return' && $is_cc
			   ? to_c_return_type $d
			   : $d->{type});
    $p->{use_name} = true    unless exists $p->{use_name};
    $p->{pos}      = 'other' unless exists $p->{pos};

    my $name = $t->{name};
    my $type = $t->{type};

    return ( (to_type_name {%$d, type=>'string'}, $p, %$accum) ,
	     (to_type_name {%$d, type=>'int', name=>"$d->{name}_size"}, $p, %$accum) )
      if $name eq 'encoded string' && $is_cc && $pos eq 'parm';

    my $str;

    $str .= "const " if $t->{const};

    if ($name eq 'string') {
      if ($is_native && $pos eq 'parm') {
	$accum->{headers}{'parm string'} = true;
	$str .= "ParmString";
      } else {
	$str .= "const char *";
      }
    } elsif ($name eq 'string obj') {
      die unless $pos eq 'return';
      if ($is_cc) {
	$str .= "const char *";
      } else {
	$accum->{headers}{'string'} = true;
	$str .= "String";
      }
    } elsif ($name eq 'encoded string') {
      $str .= "const char *";
    } elsif ($name eq '') {
      $str .= "void";
    } elsif ($name eq 'bool' && $is_cc) {
      $str .= "int";
    } elsif ($type eq 'basic') {
      $str .= $name;
    } elsif (one_of $type, qw(enum class struct union)) {
      my $c_type = $type eq 'class' ? 'struct' : $type;
      if ($t->{pointer}) {
	$accum->{types}->{$name} = $t;
      } else {
	$accum->{headers}->{$t->{created_in}} = true;
      }
      $str .= "$c_type Aspell" if $mode eq 'cc';
      $str .= to_mixed($name);
    } else {
      print STDERR "Warning: Unknown Type: $name\n";
      $str .= "{unknown type: $name}";
    }

    $str .= " *" if $t->{pointer};

    $str .= " ".to_lower($d->{name}) if defined $d->{name} && $p->{use_name};

    $str .= "[$t->{array}]" if $t->{array};

    return $str;
  }

  sub make_desc ( $ ; $) {
    my ($desc, $indent) = @_;
    return '' unless defined $desc;
    my @desc = split /\n/, $desc;
    $indent = 0 unless defined $indent;
    $indent = ' 'x$indent;
    return ("$indent/* ".
	    join("\n$indent * ", @desc).
	    " */\n");
  }

  sub make_func ( $ $ \@ $ ; \% ) {
    my ($name, $desc, $d, $p, $accum) = @_;
    $accum = {} unless defined $accum;
    my @d = @$d;
    return join '',
      ("\n",
       make_desc($desc),
       to_type_name(shift @d, {%$p,pos=>'return'}, %$accum),
       ' ',
       to_lower $name,
       '(',
       (join ', ', map {to_type_name $_, {%$p,pos=>'parm'}, %$accum} @d),
       ')');
  }

  sub make_c_func ( $ $ \@ $ ; \% ) {
    my ($name, $desc, $d, $p, $accum) = @_;
    $accum = {} unless defined $accum;
    $p->{use_name} = false unless exists $p->{use_name};
    $name = "aspell $name" unless $name =~ /aspell/;
    $name =~ s/aspell\ ?// if exists $p->{no_aspell};
    return make_func $name, $desc, @$d, $p, %$accum;
  }

  sub make_c_method ($ $ $ ; \% ) {
    my ($class, $d, $p, $accum) = @_;
    $accum = {} unless defined $accum;
    my $mode = $p->{mode};
    my $name = $d->{name};
    my $func = '';
    my $desc = $d->{desc};
    my @data = ();
    @data = @{$d->{data}} if defined $d->{data};
    if ($d->{type} eq 'constructor') {
      if (defined $name) {
	$func = $name;
      } else {
	$func = "new aspell $class";
      }
      splice @data, 0, 0, {type => $class} unless exists $d->{'returns alt type'};
      return make_c_func $func, $desc, @data, $p, %$accum;
    } elsif ($d->{type} eq 'destructor') {
      $func = "delete aspell $class";
      splice @data, 0, 0, ({type => 'void'}, {type => $class, name=>'ths'});
      return make_c_func $func, $desc, @data, $p, %$accum;
    } elsif ($d->{type} eq 'method') {
      if (exists $d->{'c func'}) {
	$func = $d->{'c func'};
      } elsif (exists $d->{'prefix'}) {
	$func = "$d->{prefix} $name";
      } else {
	$func = "aspell $class $name";
      }
      if (exists $d->{'const'}) {
	splice @data, 1, 0, {type => "const $class", name=>'ths'};
      } else {
	splice @data, 1, 0, {type => "$class", name=>'ths'};
      }
      return make_c_func $func, $desc, @data, $p, %$accum;
    } else {
      return undef;
    }
  }

  sub make_cxx_method ( $ ; \% ) {
    my ($d, $accum) = @_;
    my $ret;
    $ret .= make_func $d->{name}, $d->{desc}, @{$d->{data}}, {mode=>'native'}, %$accum;
    $ret .= " const" if exists $d->{const};
    return $ret;
  }

  sub make_cxx_constructor ( $ $ ; \% ) {
    my ($class, $p, $accum) = @_;
    my $ret;
    $ret .= to_mixed($class);
    $ret .= "(";
    $ret .= join ', ', map {to_type_name $_, {mode=>'native',pos=>'parm'}, %$accum} @$p;
    $ret .= ")";
    return $ret;
  }

  #
  # Pure C
  #

  $info{group}{proc}{cc} = sub {
    my ($data) = @_;
    my $ret;
    my $stars = (70 - length $data->{name})/2;
    $ret .= "/";
    $ret .= '*'x$stars;
    $ret .= " $data->{name} ";
    $ret .= '*'x$stars;
    $ret .= "/\n";
    foreach my $d (@{$data->{data}}) {
      $ret .= "\n\n";
      $ret .= $info{$d->{type}}{proc}{cc}->($d);
    }
    $ret .= "\n\n";
    return $ret;
  };

  $info{enum}{proc}{cc} = sub {
    my ($d) = @_;
    my $n = "Aspell".to_mixed($d->{name});
    return ("\n".
	    make_desc($d->{desc}).
	    "enum $n {" .
	    join(', ',
		 map {"Aspell".to_mixed($d->{prefix}).to_mixed($_->{type})}
		 @{$d->{data}}).
	    "};\n" .
	    "typedef enum $n $n;\n"
	   );
  };

  sub make_c_object ( $ @ ) {
    my ($t, $d) = @_;
    my $struct;
    $struct .= "Aspell";
    $struct .= to_mixed($d->{name});
    return (join "\n\n", grep {$_ ne ''}
	    join ('',
		  "$t $struct {\n",
		  (map {"\n".make_desc($_->{desc},2).
			"  ".to_type_name($_, {mode=>'cc'}). ";\n"}
		   grep {$_->{type} ne 'method'
			   && $_->{type} ne 'cxx constructor'}
		   @{$d->{data}}),
		  "\n};\n"),
	    "typedef $t $struct $struct;",
	    join ("\n",
		  map {make_c_method($d->{name}, $_, {mode=>'cc'}).";"}
		  grep {$_->{type} eq 'method'}
		  @{$d->{data}})
	   )."\n";
  }

  $info{struct}{proc}{cc} = sub {
    return make_c_object "struct", @_;
  };

  $info{union}{proc}{cc} = sub {
    return make_c_object "union", $_[0];
  };

  $info{class}{proc}{cc} = sub {
    my ($d) = @_;
    my $class = $d->{name};
    my $classname = "Aspell".to_mixed($class);
    return join("\n",
		"typedef struct $classname $classname;",
		'',
		(grep {defined $_} 
		 map {make_c_method($class, $_, {mode=>'cc'}).";"}
		 @{$d->{data}}),
		''
	       );
  };

  $info{errors}{proc}{cc} = sub {
    my ($d) = @_;
    my $p;
    my $ret;
    $p = sub {
      my ($level, $data) = @_;
      return unless defined $data;
      foreach my $d (@$data) {
	$ret .= "extern const struct AspellErrorInfo * const ";
	$ret .= ' 'x$level;
	$ret .= "perror_";
	$ret .= to_lower($d->{type});
	$ret .= ";\n";
	$p->($level + 2, $d->{data});
      }
    };
    $p->(0, $d->{data});
    return $ret;
  };

  #
  # C for C++
  #

  $info{group}{proc}{native} = sub {
    my ($data) = @_;
    return if exists $data->{'no native'};
    create_cc_file (type => 'native',
		    cxx => true,
		    dir => "common",
		    header => true,
		    data => $data);
  };

  $info{enum}{proc}{native} = sub {
    my ($data) = @_;
    my $n = to_mixed($data->{name});
    return ("enum $n {" .
	    join (',',
		 map {to_mixed($data->{prefix}).to_mixed($_->{type})}
		 @{$data->{data}}).
	    "};\n");
  };

  sub make_native_obj ( $ @ ) {
    my ($t, $data, $accum) = @_;
    my $obj = to_mixed($data->{name});
    my @defaults;
    my @public;
    foreach my $d (@{$data->{data}}) {
      next unless $d->{type} eq 'public';
      next if $d->{name} eq $data->{name};
      push @public, to_mixed($d->{name});
      my $typ = finalized_type $d->{name};
      $accum->{headers}{$typ->{created_in}} = true;
    }
    my $ret;
    $ret .= "$t $obj ";
    $ret .= ": ".join(', ', map {"public $_"} @public).' ' if @public;
    $ret .= "{\n";
    $ret .= " public:\n" if $t eq 'class';
    foreach my $d (@{$data->{data}}) {
      next if exists $d->{'c only'};
      next if one_of $d->{type}, qw(constructor destructor public);
      $ret .= "  ";
      if ($d->{type} eq 'method') {
	my $is_vir = $t eq 'class' && !exists $d->{'cxx impl'};
	$ret .= "virtual " if $is_vir;
	$ret .= make_cxx_method $d, %$accum;
	$ret .= $is_vir ? " = 0;\n" 
	  : exists $d->{'cxx impl'} ? " { $d->{'cxx impl'}; }\n"
	    : ";\n";
      } elsif ($d->{type} eq 'cxx constructor') {
	$ret .= make_cxx_constructor $data->{name}, $d->{data}, %$accum;
	$ret .= exists $d->{'cxx impl'} ? " $d->{'cxx impl'}\n"
	  : ";\n";
      } else { # is a type
	if (exists $d->{default}) {
	  push @defaults, $d;
	}
	if ($d->{type} eq 'cxx member') {
	  foreach (split /\s*,\s*/, $d->{'headers'}) {
	    $accum->{headers}{$_} = true;
	  }
	  $ret .= $d->{what};
	} elsif ($t eq 'class') {
	  $ret .= to_type_name $d, {mode=>'native'}, %$accum;
	  $ret .= "_";
	} else {
	  $ret .= to_type_name $d, {mode=>'cc_cxx'}, %$accum;
	}
	$ret .= ";\n";
      }
    }
    if (@defaults || $t eq 'class') {
      $ret .= "  $obj()";
      if (@defaults) {
	$ret .= " : ";
	$ret .= join ', ', map {to_lower($_->{name}).($t eq 'class'?'_':'')."($_->{default})"} @defaults;
      }
      $ret .= " {}\n";
    }
    $ret .= "  virtual ~${obj}() {}\n" if $t eq 'class';
    $ret .= "};\n";
    foreach my $d (@{$data->{data}}) {
      next unless $d->{type} eq 'constructor';
      $ret .= make_c_method $data->{name}, $d, {mode=>'native',no_aspell=>false}, %$accum;
      $ret .= ";\n";
    }
    return $ret;
  }

  $info{struct}{proc}{native} = sub {
    return make_native_obj 'struct', @_;
  };

  $info{union}{proc}{native} = sub {
    return make_native_obj 'union', @_;
  };

  $info{class}{proc}{native} = sub {
    return make_native_obj 'class', @_;
  };

  $info{errors}{proc}{native} = sub {
    my ($data, $accum) = @_;
    my $ret;
    $accum->{types}{"error info"} = finalized_type "error info";
    my $p0;
    $p0 = sub {
      my ($level, $data) = @_;
      return unless defined $data;
      foreach my $d (@$data) {
	$ret .= "extern \"C\" const ErrorInfo * const ";
	$ret .= ' 'x$level;
	$ret .= "perror_";
	$ret .= to_lower($d->{type});
	$ret .= ";\n";
	$p0->($level + 2, $d->{data});
      }
    };
    my $p1;
    $p1 = sub {
      my ($level, $data) = @_;
      return unless defined $data;
      foreach my $d (@$data) {
	$ret .= "static const ErrorInfo * const ";
	$ret .= ' 'x$level;
	$ret .= to_lower($d->{type});
	$ret .= "_error" if defined $d->{data} || $level == 0;
	$ret .= " = perror_";
	$ret .= to_lower($d->{type});
	$ret .= ";\n";
	$p1->($level + 2, $d->{data});
      }
    };
    $p0->(0, $data->{data});
    $ret .= "\n\n";
    $p1->(0, $data->{data});
    return $ret;
  };

  #
  # Impl
  #

  $info{group}{proc}{impl} = sub {
    my ($data) = @_;
    create_cc_file (type => 'impl',
		    cxx => true,
		    dir => "lib",
		    pre_ext => "-c",
		    header => false,
		    data => $data,
		    accum => {headers => {$data->{name} => true} }
		   );
  };

  $info{class}{proc}{impl} = sub {
    my ($data, $accum) = @_;
    my $ret;
    foreach (grep {$_ ne ''} split /\s*,\s*/, $data->{'c impl headers'}) {
      $accum->{headers}{$_} = true;
    }
    foreach my $d (@{$data->{data}}) {
      next unless one_of $d->{type}, qw(method constructor destructor);
      my @parms = @{$d->{data}} if exists $d->{data};
      my $m = make_c_method $data->{name}, $d, {mode=>'cc_cxx', use_name=>true}, %$accum;
      next unless defined $m;
      $ret .= "extern \"C\" $m\n";
      $ret .= "{\n";
      if (exists $d->{'c impl'}) {
	$ret .= cmap {"  $_\n"} split /\n/, $d->{'c impl'};
      } else {
	if ($d->{type} eq 'method') {
	  my $ret_type = shift @parms;
	  my $ret_native = to_type_name $ret_type, {mode=>'native_no_err', pos=>'return'}, %$accum;
	  my $snum = 0;
	  foreach (@parms) {
	    my $n = to_lower($_->{name});
	    if ($_->{type} eq 'encoded string') {
	      $accum->{headers}{'mutable string'} = true;
	      $accum->{headers}{'convert'} = true;
	      $ret .= "  ths->temp_str_$snum.clear();\n";
	      $ret .= "  ths->from_encoded_->convert($n, ${n}_size, ths->temp_str_$snum);\n";
	      $ret .= "  ths->temp_str_$snum.append('\\0');\n";
	      $ret .= "  unsigned int s$snum = ths->temp_str_$snum.size();\n";
	      $ret .= "  ths->from_encoded_->append_null(ths->temp_str_$snum);\n";
	      $_ = "MutableString(ths->temp_str_$snum.data(), s$snum)";
	      $snum++;
	    } else {
	      $_ = $n;
	    }
	  }
	  my $parms = '('.(join ', ', @parms).')';
	  my $exp = "ths->".to_lower($d->{name})."$parms";
	  if (exists $d->{'posib err'}) {
	    $accum->{headers}{'posib err'} = true;
	    $ret .= "  PosibErr<$ret_native> ret = $exp;\n";
	    $ret .= "  ths->err_.reset(ret.release_err());\n";
	    $ret .= "  if (ths->err_ != 0) return ".(c_error_cond $ret_type).";\n";
	    if ($ret_type->{type} eq 'void') {
	      $ret_type = {type=>'special'};
	      $exp = "1";
	    } else {
	      $exp = "ret.data"
	    }
	  }
	  if ($ret_type->{type} eq 'string obj') {
	    $ret .= "  ths->temp_str = $exp;\n";
	    $exp = "ths->temp_str.c_str()";
	  } elsif ($ret_type->{type} eq 'encoded string') {
	    die; 
	    # This is not used and also not implemented right
	    $ret .= "  if (to_encoded_ != 0) (*to_encoded)($exp,temp_str_);\n";
	    $ret .= "  else                  temp_str_ = $exp;\n";
	    $exp = "temp_str_0.data()";
	  }
	  $ret .= "  ";
	  $ret .= "return " unless $ret_type->{type} eq 'void';
	  $ret .= $exp;
	  $ret .= ";\n";
	} elsif ($d->{type} eq 'constructor') {
	  my $name = $d->{name} ? $d->{name} : "new $data->{name}";
	  $name =~ s/aspell\ ?//; # FIXME: Abstract this in a function
	  $name = to_lower($name);
	  shift @parms if exists $d->{'returns alt type'}; # FIXME: Abstract this in a function
	  my $parms = '('.(join ', ', map {$_->{name}} @parms).')';
	  $ret .= "  return $name$parms;\n";
	} elsif ($d->{type} eq 'destructor') {
	  $ret .= "  delete ths;\n";
	}
      }
      $ret .= "}\n\n";
    }
    return $ret;
  };

  $info{struct}{proc}{impl} = $info{class}{proc}{impl};

  $info{union}{proc}{impl} = $info{class}{proc}{impl};

  #
  # Native Impl
  #

  $info{group}{proc}{native_impl} = sub {
    my ($data) = @_;
    create_cc_file (type => 'native_impl',
		    cxx => true,
		    dir => "common",
		    header => false,
		    data => $data,
		    accum => {headers => {$data->{name} => true} }
		   );
  };

  $info{errors}{proc}{native_impl} = sub {
    my $ret;
    my $p;
    $p = sub {
      my ($isa, $parms, $data) = @_;
      my @parms = (@$parms, (split /, */, $data->{parms}));
      my $parm_idx = sub {
	my ($p) = @_;
	return 0             if $p eq 'prim';
	for (my $i = 0; $i != @parms; ++$i) {
	  return $i+1 if $parms[$i] eq $p;
	}
	die "can't find parm for \"$p\"";
      };
      my $proc_mesg = sub {
	my @mesg = split /\%(\w+)/, $_[0];
	my $mesg = '';
	while (true) {
	  my $m = shift @mesg;
	  $m =~ s/\"/\\\"/g;
	  $mesg .= $m;
	  my $p = shift @mesg;
	  last unless defined $p;
	  $mesg .= "%$p:";
	  $mesg .= $parm_idx->($p);
	}
	if (length $mesg == 0) {
	  $mesg = 0;
	} else {
	  $mesg = "\"$mesg\"";
	}
	return $mesg;
      };
      my $mesg = $proc_mesg->($data->{mesg});
      my $name = "perror_".to_lower($data->{type});
      $ret .= "static const ErrorInfo $name\_obj = {\n";
      $ret .= "  ".(defined $isa ? "$isa": 0).", // isa\n";
      $ret .= "  $mesg, // mesg\n";
      $ret .= "  ".scalar @parms.", // num_parms\n";
      $ret .= "  {".(join ', ', map {"\"$_\""} @parms)."} // parms\n";
      $ret .= "};\n";
      $ret .= "const ErrorInfo * const $name = &$name\_obj;\n";
      $ret .= "\n";
      foreach my $d (@{$data->{data}}) {
	$ret .= $p->($name, \@parms, $d);
      }
    };
    my ($data, $accum) = @_;
    $accum->{headers}{'error'} = true;
    foreach my $d (@{$data->{data}}) {
      $ret .= $p->(undef, [], $d);
    }
    return $ret;
  };

}

######################################################################
#
# Util Functions
#

#
# Parser helper functions
#

sub need_options ( $ ) {
  my ($i, $o) = @_;
  my $options = $info{$i->{type}}{options};
  return true unless ref $options eq 'ARRAY';
  return true unless @$options == 0;
  return false;
}

sub valid_option ( $ $ ) {
  my ($i, $o) = @_;
  my $options = $info{$i->{type}}{options};
  return true unless ref $options eq 'ARRAY';
  return scalar ( grep {$_ eq $o} @$options);
}

sub valid_group ( $ $ ) {
  my ($i, $t) = @_;
  my $groups = $info{$i->{type}}{groups};
  return true unless ref $groups eq 'ARRAY';
  return scalar ( grep {$_ eq $t} @$groups);
}

sub store_group ( $ $ ) {
  my ($d, $data) = @_;
  if ($d->{type} eq 'methods') {
    $methods{"$d->{name} methods"} = $d;
#   Don't usa as groups is now undef
#   push @{$info{class}{groups}}, "$d->{name} methods";
  } else {
    push @{$data->{data}}, $d;
  }
}

#
# Copy Methods
#

sub copy_n_sub ( $ $ );

sub copy_methods ( $ $ $ ) {
  my ($d, $data, $class_name) = @_;
  my $ms = $methods{$d->{type}};
  if (not defined $d->{name}) {
    $d->{name} = $class_name;
    $d->{name} =~ s/ [^ ]+$// if $ms->{strip} == 1;
  }
  my @lst;
  if (defined $ms->{'c impl headers'}) {
    $data->{'c impl headers'} .= ",$ms->{'c impl headers'}";
  }
  foreach my $m (@{$ms->{data}}) {
    push @lst, copy_n_sub($m, $d->{name});
    $lst[-1]{prefix} = $m->{prefix} if exists $d->{prefix};
  }
  return @lst
}

sub copy_n_sub ( $ $ ) {
  my ($d, $name) = @_;
  my $new_d = {};
  foreach my $k (keys %$d) {
    if ($k eq 'data') {
      $new_d->{data} = [];
      foreach my $d0 (@{$d->{data}}) {
	push @{$new_d->{data}}, copy_n_sub($d0, $name);
      }
    } else {
      $new_d->{$k} = $d->{$k};
      $new_d->{$k} =~ s/\$/$name/g unless ref $new_d->{$k};
    }
  }
  return $new_d;
}

#
# Type and interface generation helper functions
#

sub to_upper( $ ) {local ($_) = @_;
		   s/^\s+//; s/\s+$//; s/(\S)\s+(\S)/$1_$2/g;
		   return uc($_);
		 }
sub to_lower( $ ) {local ($_) = @_;
		   s/^\s+//; s/\s+$//; s/(\S)\s+(\S)/$1_$2/g;
		   return lc($_);
		 }
sub to_mixed( $ ) {local ($_) = @_;
		   s/\s*(\S)(\S*)/\u$1\l$2\E/g;
		   return $_;
		 }

#
# Type Functions
#

sub creates_type ( $ ) {
  my ($i) = @_;
  my $d;
  $d->{type} = $info{$i->{type}}{creates_type};
  return undef unless defined $d->{type};
  $d->{name} = $i->{name};
  $d->{treat_as} =
    ($i->{type} eq 'basic'                                    ? 'special'
     : exists $i->{'treat as object'} || $i->{type} eq 'enum' ? 'object'
     :                                                          'pointer');
  if (my $name = $info{$i->{type}}{creates_name}) {
    $d->{name} = $name->($i);
  }
  return $d;
}

sub update_type ( $ ; $ ) {
  my ($name, $data) = @_;
  my $d = $types{$name};
  $types{$name} = $d = {} unless defined $d;
  $d->{data} = $data if defined $data;
  $d->{data} = {} unless defined $d->{data};
  return $d;
}

sub finalized_type ( $ ) {

  my ($name) = @_;

  my $d = $types{$name};
  $types{$name} = $d = {data=>{}} unless defined $d;
  return $d unless exists $d->{data};

  while (my ($k,$v) = each %{$d->{data}}) {
    $d->{$k} = defined $v ? $v : true;
  }
  delete $d->{data};

  local $_ = $name;

  s/^const //       and $d->{const}   = true;
  s/^array (\d+) // and $d->{array}   = $1;
  s/ ?pointer$//    and $d->{pointer} = true;
  s/ ?object$//     and $d->{pointer} = false;

  $_ = 'void' if length $_ == 0;

  my $r = finalized_type $_;

  $d->{type} = exists $r->{type} ? $r->{type} : 'unknown';
  $d->{name} = $_;
  $d->{orig_name} = $name;
  $d->{pointer} = ($r->{treat_as} eq 'pointer')
    unless exists $d->{pointer};
  $d->{const} = false unless $d->{pointer};
  $d->{created_in} = $r->{created_in};

  return $d;

}

#
# Create File
#

sub create_file ( $ $ ) {
  my ($filename, $to_write) = @_;
  local $/ = undef;
  my $existing = '';
  open F,$filename and $existing=<F>;
  if ($to_write eq $existing) {
    print "File \"$filename\" unchanged.\n";
  } elsif (length $existing > 0 && $existing !~ /Automatically generated file\./) {
    print "Will not write over \"$filename\".\n";
  } else {
    print "Creating \"$filename\".\n";
    open F, ">$filename" or die;
    print F $to_write;
  }
}
