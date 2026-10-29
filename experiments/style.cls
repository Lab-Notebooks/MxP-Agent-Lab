\NeedsTeXFormat{LaTeX2e}
\ProvidesClass{style}[2026-10-28 lab notebook style]

\LoadClass[11pt]{article}

\RequirePackage[paperwidth=8.5in,paperheight=11in,textwidth=5.6in,top=0.9in,bottom=0.9in]{geometry}
\RequirePackage[T1]{fontenc}
\RequirePackage{lmodern,microtype,graphicx,booktabs,xcolor,enumitem,tcolorbox,float}
\RequirePackage[most]{tcolorbox}

\definecolor{accent}{HTML}{2A78D6}
\definecolor{ink}{HTML}{0B0B0B}
\definecolor{muted}{HTML}{52514E}
\definecolor{wash}{HTML}{F3F2EE}

\setlength{\parindent}{0pt}
\setlength{\parskip}{0.6em}

\newcommand{\f}[1]{\texttt{#1}}
\newtcolorbox{panel}[1][]{enhanced,colback=wash,colframe=wash,arc=4pt,left=10pt,right=10pt,#1}