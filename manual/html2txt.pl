$what = $ARGV[0];

opendir DIR, "$what-html";

$/ = undef;

foreach (readdir(DIR)) {
  next unless /(.+?)\.html$/;
  $command = "$ENV{'TEXT_DUMP'} -dump -$ENV{'DUMP_WIDTH'} 75 $what-html/$_  > $what-text/$1.txt";
  $name = $1;
  print "$command\n" ;
  system $command;
  open IN, "$what-text/$name.txt";
  $in = <IN>;
  $in =~ tr/\240/ /;
  $in =~ s/``|''/\"/g;
  $in =~ s/^\s+next up previous contents.+?\n\n(.+\n---+\n).+?$/$1/s;
  open IN, ">$what-text/$name.txt";
  print IN $in;
}
