#----------------------------------------------------------------------
# Makefile.am  -  FINAL CUT example programs
#----------------------------------------------------------------------

LIBS = -lfinal

if ! CPPUNIT_TEST

AM_LDFLAGS = -L$(top_builddir)/final/.libs
AM_CPPFLAGS = -I$(top_srcdir)/final -Wall -Werror -std=c++14

noinst_PROGRAMS = \
	7segment \
	background-color \
	busy \
	calculator \
	cartesian_graph \
	checklist \
	choice \
	dialog \
	event-log \
	eventloop \
	fullwidth-character \
	hello \
	highlight-text \
	input-dialog \
	keyboard \
	listbox \
	listview \
	mandelbrot \
	menu \
	mouse \
	opti-move \
	parallax-scrolling \
	rotozoomer \
	scrollview \
	string-operations \
	term-attributes \
	termcap \
	terminal-focus \
	timer \
	transparent \
	treeview \
	ui \
	watch \
	widget-colors \
	windows \
	xpmview

7segment_SOURCES = 7segment.cpp
background_color_SOURCES = background-color.cpp
busy_SOURCES = busy.cpp
calculator_SOURCES = calculator.cpp
cartesian_graph_SOURCES = cartesian_graph.cpp
checklist_SOURCES = checklist.cpp
choice_SOURCES = choice.cpp
dialog_SOURCES = dialog.cpp
event_log_SOURCES = event-log.cpp
eventloop_LDADD = -lpthread
eventloop_SOURCES = eventloop.cpp
fullwidth_character_SOURCES = fullwidth-character.cpp
hello_SOURCES = hello.cpp
highlight_text_SOURCES = highlight-text.cpp
input_dialog_SOURCES = input-dialog.cpp
keyboard_SOURCES = keyboard.cpp
listbox_SOURCES = listbox.cpp
listview_SOURCES = listview.cpp
mandelbrot_SOURCES = mandelbrot.cpp
menu_SOURCES = menu.cpp
mouse_SOURCES = mouse.cpp
opti_move_SOURCES = opti-move.cpp
parallax_scrolling_SOURCES = parallax-scrolling.cpp
rotozoomer_SOURCES = rotozoomer.cpp
scrollview_SOURCES = scrollview.cpp
string_operations_SOURCES = string-operations.cpp
term_attributes_SOURCES = term-attributes.cpp
termcap_SOURCES = termcap.cpp
terminal_focus_SOURCES = terminal-focus.cpp
timer_SOURCES = timer.cpp
transparent_SOURCES = transparent.cpp
treeview_SOURCES = treeview.cpp
ui_SOURCES = ui.cpp
watch_SOURCES = watch.cpp
widget_colors_SOURCES = widget-colors.cpp
windows_SOURCES = windows.cpp
xpmview_SOURCES = xpmview.cpp

endif

clean-local:
	-find . \( -name "*.gcda" -o -name "*.gcno" -o -name "*.gcov" \) -delete
	-rm -rf .deps
