sub prep_str($)
{
  local $_ = $_[0];
  s/\\(.)/$1/g;
  s/([\"\\])/\\$1/g;
  return $_;
}

%filters=();
while ($filename=shift) {
  ( open OPTIONFILE,"<$filename") || 
   (printf STDERR "can't open `$filename'; ignored;\n") && next;
  $filtername=$filename;
  $filtername=~s/-filter\.opt$//;
  $filtername=~s/[^\/]*\///g;
  ( exists $filters{$filtername}) &&
   (printf STDERR "filter allready defined $filtername($filename); ignored;\n") && next;
  $filter=$filters{$filtername}={};
  ${$filter}{"NAME"}=$filtername;
  ${$filter}{"DECODER"}="0";
  ${$filter}{"FILTER"}="0";
  ${$filter}{"ENCODER"}="0";
  ${$filter}{"DESCRIPTION"}="";
  $inoption=0;
  while (<OPTIONFILE>) {
    chomp;
    (($_=~/^\#/) || ($_=~/^[ \t]*$/)) && next;
    $_=~s/[ \t]+$//;
    $_=~s/^[ \t]+//;
    ($_=~s/^ENDFILE(?:[ \t]+|$)//i) && last;
    unless ($inoption) {
      ( $_=~s/^DES(?:CRIPTION)[ \t]+//i) && (($_=~s/\\(?=[ \t])//g) || 1) &&
       (${$filter}{"DESCRIPTION"}=$_) && next;
      ( $_=~s/^STATIC[ \t]+//i) && (($feature=uc $_ ) || 1) &&
       (${$filter}{$feature}="new_".$filtername."_".(lc $_)) && next;
      ( $_=~s/^ASPELL[ \t]+//i) && next;
      ( $_=~/^OPTION[ \t]+/i) || 
       (die "Invalid general key in $filename on line $.");
    }
    if ($_=~s/^OPTION[ \t]+//i) {
      $inoption=1;
      $option=${$filter}{$_}={};
      ${$option}{"NAME"}=$_;
      ${$option}{"TYPE"}="KeyInfoBool";
      ${$option}{"DEFAULT"}="";
      ${$option}{"DESCRIPTION"}="";
      next;
    }
    ( $_=~s/^TYPE[ \t]+//i) &&
     (${$option}{"TYPE"}=$_) && next;
    ( $_=~s/^DEF(?:AULT)[ \t]+//i) && (($_=~s/\\(?=[ \t])//g) || 1) &&
     (((${$option}{"DEFAULT"} ne "") && (${$option}{"DEFAULT"}.=",")) ||1) &&
     (${$option}{"DEFAULT"}.=prep_str($_)) && next;
    ( $_=~s/^DES(?:CRIPTION)[ \t]+//i) && (($_=~s/\\(?=[ \t])//g) || 1) &&
     (${$option}{"DESCRIPTION"}=prep_str($_)) && next;
    ( $_=~s/^ENDOPTION(?:[ \t]+|$)//i) && 
     (($inoption=0)||1) && next;
    ( $_=~s/^STATIC[ \t]+//i) && (($feature=uc $_ ) || 1) &&
     (${$filter}{$feature}="new_".$filtername."_".(lc $_)) && 
     (($inoption=0)||1) && next;
    die "Invalid option key in $filename on line $.";
  }
  close OPTIONFILE;
}

(scalar (@allfilters = keys %filters)) || exit 0;
open STATICFILTERS, ">gen/static_filters.src.cpp" || die "cant generate static filter description\n";
printf STATICFILTERS "/*File generated during static filter build\n".
                     "  Automatically generated file\n*/\n";
@rallfilters=();
while ($filter = shift @allfilters) {
  ( $filters{$filter}{"DECODER"} ne  "0") &&
   (printf STATICFILTERS "\n  IndividualFilter * ".
    $filters{$filter}{"DECODER"}."();\n");
  ( $filters{$filter}{"FILTER"} ne "0") &&
   (printf STATICFILTERS "\n  IndividualFilter * ".
                         $filters{$filter}{"FILTER"}."();\n");
  ( $filters{$filter}{"ENCODER"} ne "0") &&
   (printf STATICFILTERS "\n  IndividualFilter * ".
                         $filters{$filter}{"ENCODER"}."();\n");
  push @rallfilters,$filter;
}
@allfilters=(@rallfilters);
printf STATICFILTERS "\n  static FilterEntry standard_filters[] = {\n";
@filterhashes=();
@rallfilters=();
while ($filter = shift @allfilters) {
  push @filterhashes,$filters{$filter};
  (scalar @rallfilters) && (printf STATICFILTERS ",\n");
  printf STATICFILTERS "    {\"$filter\",".$filters{$filter}{"DECODER"}.
                                     ",".$filters{$filter}{"FILTER"}.
                                     ",".$filters{$filter}{"ENCODER"}."}";
  push @rallfilters,$filter;
}
printf STATICFILTERS "\n  };\n";
printf STATICFILTERS "\n  const unsigned int standard_filters_size = ".
                         "sizeof(standard_filters)/sizeof(FilterEntry);\n";
printf STATICFILTERS "\n  static KeyInfo modes_module[] = {\n";
printf STATICFILTERS "    {\"fm-email\",KeyInfoList,\"url,email\",0},\n";
printf STATICFILTERS "    {\"fm-none\",KeyInfoList,\"\",0},\n";
printf STATICFILTERS "    {\"fm-sgml\",KeyInfoList,\"url,sgml\",0},\n";
printf STATICFILTERS "    {\"fm-tex\",KeyInfoList,\"url,tex\",0},\n";
printf STATICFILTERS "    {\"fm-url\",KeyInfoList,\"url\",0},\n  };\n";
printf STATICFILTERS "\n  const KeyInfo * modes_module_begin = modes_module;\n";
printf STATICFILTERS "\n  const KeyInfo * modes_module_end = modes_module+".
                        "sizeof(modes_module)/sizeof(KeyInfo);\n";
while ($filter = shift @filterhashes) {
  printf STATICFILTERS "\n  static KeyInfo ".${$filter}{"NAME"}."_options[] = {\n";
  printf STATICFILTERS "    {\"filter-".${$filter}{"NAME"}."\",KeyInfoDescript,0,\"".
                                        ${$filter}{"DESCRIPTION"}."\"}";
  while (($name,$option)=each %{$filter}) {
    ($name=~/(?:NAME|(?:DE|EN)CODER|FILTER|DESCRIPTION)/) && next;
    printf STATICFILTERS ",\n    {\"$name\",";
    ((lc ${$option}{"TYPE"}) eq "bool") && printf STATICFILTERS "KeyInfoBool,";
    ((lc ${$option}{"TYPE"}) eq "int") && printf STATICFILTERS "KeyInfoInt,";
    ((lc ${$option}{"TYPE"}) eq "string") && printf STATICFILTERS "KeyInfoString,";
    ((lc ${$option}{"TYPE"}) eq "list") && printf STATICFILTERS "KeyInfoList,";
    print STATICFILTERS "\"".${$option}{"DEFAULT"}."\",\""
                            .${$option}{"DESCRIPTION"}."\"}";
  }
  printf STATICFILTERS "\n  };\n";
  printf STATICFILTERS "\n  const KeyInfo * ".${$filter}{"NAME"}."_options_begin = ".
                                              ${$filter}{"NAME"}."_options;\n";
  printf STATICFILTERS "\n  const KeyInfo * ".${$filter}{"NAME"}."_options_end = ".
                                              ${$filter}{"NAME"}."_options+sizeof(".
                                              ${$filter}{"NAME"}."_options)/".
                                              "sizeof(KeyInfo);\n";
}
printf STATICFILTERS "\n\n  static ConfigModule filter_modules[] = {\n";
printf STATICFILTERS "    {\"fm\",0,modes_module_begin,modes_module_end}";
while ($filter = shift @rallfilters) {
  printf STATICFILTERS ",\n    {\"$filter\",0,${filter}_options_begin,${filter}_options_end}";
} 
printf STATICFILTERS "\n  };\n";
printf STATICFILTERS "\n  const ConfigModule * filter_modules_begin = ".
                         "filter_modules;\n";
printf STATICFILTERS "\n  const ConfigModule * filter_modules_end = ".
                         "filter_modules+sizeof(filter_modules)/".
                         "sizeof(ConfigModule);\n";
printf STATICFILTERS "\n  const size_t filter_modules_size = ".
                          "sizeof(filter_modules);\n";

close STATICFILTERS;

