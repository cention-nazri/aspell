
$what = $ARGV[0];

print ">>$what\n";

$/ = undef;
open DOC, "${what}2.tex";
$doc = <DOC>;

open IN, "fdl.tex";
$fdl = <IN>;

open IN, "mk-src.tex";
$mksrc = <IN>;
$mksrc =~ s/\\subsection/\\subsubsection/g;
$mksrc =~ s/\\section/\\subsection/g;

open IN, "lgpl.txt";
$lgpl = <IN>;
$lgpl = <<"---";
\\begin{verbatim}
$lgpl
\\end{verbatim}
---

close IN;

#$doc =~ s/(http\:\/\/\S+?)([\.\)]\W|\s)/\\htmladdnormallink{$1}{$1}$2/g;

$doc =~ s/\\IfFileExists\{url\.sty\}.+\n.+/\\usepackage{url}/;
$doc =~ s/\\url\|(.+?)\|/\\url\{$1\}/g;
$doc =~ s/(?<!-)--(?!-)/\\doubledash{}/g;
$doc =~ s/-\{\}-/\\doubledashb{}/g;
$doc =~ s/-\{\}-\{\}-/\\doubledash{}/g;
$doc =~ s/<\\textcompwordmark{}</\\dlt{}/g;
$doc =~ s/>\\textcompwordmark{}>/\\dgt{}/g;
$doc =~ s/<\\,\{\}</\\dlt{}/g;
$doc =~ s/>\\,\{\}>/\\dgt{}/g;
$doc =~ s/<</\\dlt{}/g;
$doc =~ s/>>/\\dgt{}/g;
$doc =~ s/\\textasciitilde\{\}/\\\~{}/g;
$doc =~ s/\\textasciicircum\{\}/\\\^{}/g;

$doc =~ s/\(\(FDL\)\)/$fdl/g;
$doc =~ s/\(\(LGPL\)\)/$lgpl/g;
$doc =~ s/\(\(MKSRC\)\)/$mksrc/g;

open DOC, ">$what.tex";
print DOC $doc;

