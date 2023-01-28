
%{
/*
  flex w.l && cl -D YY_NO_UNISTD_H lex.yy.c && lex.yy.exe < input.txt | iconv -f utf-8 -t cp932
*/
#include <tchar.h>
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

#include <io.h>
#define fileno( exp ) (_fileno( (exp) ))
#define isatty( exp ) (_isatty( (exp) ))
%}

%x escape
%x CONSOLETITLE
NEWLINE \r?\n
DIGIT   [0-9]
NUMBER  [1-9][0-9]*
BEL     \x07
ESC     \x1b

%%
{ESC}     BEGIN(escape);

<escape>"]"[02]";"       { BEGIN( CONSOLETITLE ); }
<CONSOLETITLE>{ESC}{BEL} { BEGIN( INITIAL ); }
<CONSOLETITLE>.          {}

<escape>"M"              {
                           /* Reverse Index - Performs the reverse operation of \n, moves cursor up one line, maintains horizontal position, scrolls buffer if necessary */
                           BEGIN( INITIAL );
                         }
<escape>"7"              {
                           /* Save Cursor Position in Memory */
                           BEGIN( INITIAL );
                         }
<escape>"8"              {
                           /* Restore Cursor Position from Memory */
                           BEGIN( INITIAL );
                         }

<escape>"[!p"            { /* soft reset */
                           BEGIN( INITIAL );
                         }
<escape>"["{NUMBER}"A"   {
                           /* Cursor up by <n> */
                           BEGIN( INITIAL );
                         }
<escape>"["{NUMBER}"B"   {
                           /* Cursor down by <n> */
                           BEGIN( INITIAL );
                         }
<escape>"["{NUMBER}"C"   {
                           /* Cursor forward by <n> */
                           BEGIN( INITIAL );
                         }
<escape>"["{NUMBER}"D"   {
                           /* Cursor background by <n> */
                           BEGIN( INITIAL );
                         }
<escape>"["{NUMBER}"E"   {
                           /* Cursor Next Line by <n> */
                           BEGIN( INITIAL );
                         }
<escape>"["{NUMBER}"F"   {
                           /* Cursor Previous Line by <n> */
                           BEGIN( INITIAL );
                         }
<escape>"["{NUMBER}"G"   {
                           /* Cursor moves to <n>th position horizontally in the current line */
                           BEGIN( INITIAL );
                         }
<escape>"["{NUMBER}"d"   {
                           /* Cursor moves to <n>th position vertically in the current column */
                           BEGIN( INITIAL );
                         }
<escape>"["{NUMBER}";"{NUMBER}"H" {
                           /* *Cursor moves to <x>; <y> coordinate within the viewport, where <x> is the column of the <y> line */
                           BEGIN( INITIAL );
                         }
<escape>"["{NUMBER}";"{NUMBER}"f" {
                           /* *Cursor moves to <x>; <y> coordinate within the viewport, where <x> is the column of the <y> line */
                           BEGIN( INITIAL );
                         }
<escape>"[s"             {
                           /* *With no parameters, performs a save cursor operation like DECSC */
                           BEGIN( INITIAL );
                         }
<escape>"[u"             {
                           /* **With no parameters, performs a restore cursor operation like DECRC */
                           BEGIN( INITIAL );
                         }
<escape>"[?12h"          {
                           /* Text Cursor Enable Blinking */
                           BEGIN( INITIAL );
                         }
<escape>"[?12l"          {
                           /* Text Cursor Disable Blinking */
                           BEGIN( INITIAL );
                         }
<escape>"[?25h"          {
                           /* Text Cursor Enable Mode Show*/
                           BEGIN( INITIAL );
                         }
<escape>"[?25l"          {
                           /* Text Cursor Disable Mode Show*/
                           BEGIN( INITIAL );
                         }
<escape>"["[0-6]" q"     {
                           /* Cursor Shape */
                           BEGIN( INITIAL );
                         }
<escape>"["{NUMBER}"S"   {
                           /* Scroll text up by <n>. Also known as pan down, new lines fill in from the bottom of the screen */
                           BEGIN( INITIAL );
                         }
<escape>"["{NUMBER}"T"   {
                           /* croll down by <n>. Also known as pan up, new lines fill in from the top of the screen */
                           BEGIN( INITIAL );
                         }

<escape>"["{NUMBER}"@"   {
                           /* Insert <n> spaces at the current cursor position, shifting all existing text to the right. Text exiting the screen to the right is removed. */
                           BEGIN( INITIAL );
                         }
<escape>"["{NUMBER}"P"   {
                           /* Delete <n> characters at the current cursor position, shifting in space characters from the right edge of the screen.*/
                           BEGIN( INITIAL );
                         }
<escape>"["{NUMBER}"X"   {
                           /* Erase <n> characters from the current cursor position by overwriting them with a space character.*/
                           BEGIN( INITIAL );
                         }
<escape>"["{NUMBER}"L"   {
                           /* Inserts <n> lines into the buffer at the cursor position. The line the cursor is on, and lines below it, will be shifted downwards. */
                           BEGIN( INITIAL );
                         }
<escape>"["{NUMBER}"M"   {
                           /* Deletes <n> lines from the buffer, starting with the row the cursor is on. */
                           BEGIN( INITIAL );
                         }

<escape>"["{NUMBER}"J"   {
                           /* Replace all text in the current viewport/screen specified by <n> with space characters */
                           BEGIN( INITIAL );
                         }
<escape>"["{NUMBER}"K"   {
                           /* Replace all text on the line with the cursor specified by <n> with space characters */
                           BEGIN( INITIAL );
                         }

<escape>"[?3h"           {
                           /* Set Number of Columns to 132 */
                           BEGIN( INITIAL );
                         }
<escape>"[?3l"           {
                           /* Set Number of Columns to 80 */
                           BEGIN( INITIAL );
                         }
<escape>"[0m"            {
                           /* Text Formatting Set Graphics Rendition Default */
                           BEGIN( INITIAL );
                         }
<escape>"[1m"            {
                           /* Text Formatting Set Graphics Rendition Bold/Bright */
                           BEGIN( INITIAL );
                         }
<escape>"[22m"           {
                           /* Text Formatting Set Graphics Rendition No Bold/Bright */
                           BEGIN( INITIAL );
                         }
<escape>"[4m"            {
                           /* Text Formatting Set Graphics Rendition Underline */
                           BEGIN( INITIAL );
                         }
<escape>"[24m"           {
                           /* Text Formatting Set Graphics Rendition No Underline */
                           BEGIN( INITIAL );
                         }
<escape>"[7m"            {
                           /* Text Formatting Set Graphics Rendition Negative */
                           BEGIN( INITIAL );
                         }
<escape>"[27m"           {
                           /* Text Formatting Set Graphics Rendition Positive */
                           BEGIN( INITIAL );
                         }
<escape>"[3"[0-9]"m"     {
                           /* Text Formatting Set Graphics Rendition Foreground */
                           BEGIN( INITIAL );
                         }
<escape>"[4"[0-9]"m"     {
                           /* Text Formatting Set Graphics Rendition Background */
                           BEGIN( INITIAL );
                         }
<escape>"[9"[0-7]"m"     {
                           /* Text Formatting Set Graphics Rendition Bright Foreground */
                           BEGIN( INITIAL );
                         }
<escape>"[10"[0-7]"m"    {
                           /* Text Formatting Set Graphics Rendition Bright Background */
                           BEGIN( INITIAL );
                         }
<escape>"[38;2;"{NUMBER}";"{NUMBER}";"{NUMBER}"m"    {
                           /* Set foreground color to RGB value specified in <r>, <g>, <b> parameters* */
                           BEGIN( INITIAL );
                         }
<escape>"[48;2;"{NUMBER}";"{NUMBER}";"{NUMBER}"m"    {
                           /* Set background color to RGB value specified in <r>, <g>, <b> parameters* */
                           BEGIN( INITIAL );
                         }
<escape>"[38;5;"{NUMBER}"m"    {
                           /* Set foreground color to <s> index in 88 or 256 color table* */
                           BEGIN( INITIAL );
                         }
<escape>"[48;5;"{NUMBER}"m"    {
                           /* Set background color to <s> index in 88 or 256 color table* */
                           BEGIN( INITIAL );
                         }

<escape>"="              {
                           /* Enable Keypad Application Mode */
                           BEGIN( INITIAL );
                         }
<escape>">"              {
                           /* Enable Keypad Numeric Mode */
                           BEGIN( INITIAL );
                         }
<escape>"[?1h"           {
                           /* Enable Cursor Keys Application Mode */
                           BEGIN( INITIAL );
                         }
<escape>"[?1l"           {
                           /* Disable Cursor Keys Application Mode */
                           BEGIN( INITIAL );
                         }
<escape>"[6n"            {
                           /* Report Cursor Position */
                           BEGIN( INITIAL );
                         }
<escape>"[0c"            {
                           /* Device Attributes */
                           BEGIN( INITIAL );
                         }
<escape>"H"              {
                           /* Horizontal Tab Set */
                           BEGIN( INITIAL );
                         }
<escape>"["{NUMBER}"I"   {
                           /* Cursor Horizontal (Forward) Tab */
                           BEGIN( INITIAL );
                         }
<escape>"["{NUMBER}"Z"   {
                           /* Cursor Backwards Tab */
                           BEGIN( INITIAL );
                         }
<escape>"[0g"            {
                           /* Tab Clear (current column) */
                           BEGIN( INITIAL );
                         }
<escape>"[3g"            {
                           /* Tab Clear (all columns) */
                           BEGIN( INITIAL );
                         }
<escape>"(0"             {
                           /* Designate Character Set - Dec Line Drawing  */
                           BEGIN( INITIAL );
                         }
<escape>"(B"             {
                           /* Designate Character Set - US ASCII */
                           BEGIN( INITIAL );
                         }
<escape>"["{NUMBER}";"{NUMBER}"r" {
                           /* Set Scrolling Region */
                           BEGIN( INITIAL );
                         }
<escape>"[?1049h"        {
                           /* Use Alternate Screen Buffer */
                           BEGIN( INITIAL );
                         }
<escape>"[?1049l"        {
                           /* Use Main Screen Buffer */
                           BEGIN( INITIAL );
                         }

{NEWLINE}                {
                           puts("");
                         }
.                        {
                           printf( "%s" , yytext );
                         }
%%

