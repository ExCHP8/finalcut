/***********************************************************************
* fvterm.cpp - Virtual terminal implementation                         *
*                                                                      *
* This file is part of the FINAL CUT widget toolkit                    *
*                                                                      *
* Copyright 2016-2021 Markus Gans                                      *
*                                                                      *
* FINAL CUT is free software; you can redistribute it and/or modify    *
* it under the terms of the GNU Lesser General Public License as       *
* published by the Free Software Foundation; either version 3 of       *
* the License, or (at your option) any later version.                  *
*                                                                      *
* FINAL CUT is distributed in the hope that it will be useful, but     *
* WITHOUT ANY WARRANTY; without even the implied warranty of           *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
* GNU Lesser General Public License for more details.                  *
*                                                                      *
* You should have received a copy of the GNU Lesser General Public     *
* License along with this program.  If not, see                        *
* <http://www.gnu.org/licenses/>.                                      *
***********************************************************************/

#include <numeric>
#include <queue>
#include <string>
#include <vector>

#include "final/fc.h"
#include "final/fcharmap.h"
#include "final/fcolorpair.h"
#include "final/flog.h"
#include "final/fpoint.h"
#include "final/frect.h"
#include "final/fsize.h"
#include "final/fstyle.h"
#include "final/fsystem.h"
#include "final/ftermbuffer.h"
#include "final/ftermoutput.h"
#include "final/ftypes.h"
#include "final/fvterm.h"

namespace finalcut
{

// static class attributes
bool                 FVTerm::draw_completed{false};
bool                 FVTerm::no_terminal_updates{false};
bool                 FVTerm::force_terminal_update{false};
const FVTerm*        FVTerm::init_object{nullptr};
FVTerm::FTermArea*   FVTerm::vterm{nullptr};
FVTerm::FTermArea*   FVTerm::vdesktop{nullptr};
FVTerm::FTermArea*   FVTerm::active_area{nullptr};
FChar                FVTerm::next_attribute{};
FChar                FVTerm::s_ch{};
FChar                FVTerm::i_ch{};


//----------------------------------------------------------------------
// class FVTerm
//----------------------------------------------------------------------

// constructors and destructor
//----------------------------------------------------------------------
FVTerm::FVTerm()
{
  if ( ! init_object )
    init();
  else
  {
    foutput = std::shared_ptr<FOutput>(init_object->foutput);
    window_list = std::shared_ptr<FVTermList>(init_object->window_list);
  }
}

//----------------------------------------------------------------------
FVTerm::FVTerm (const FVTerm& fvterm)  // copy constructor
{
  foutput = std::shared_ptr<FOutput>(fvterm.foutput);
  window_list = std::shared_ptr<FVTermList>(fvterm.window_list);
}

//----------------------------------------------------------------------
FVTerm::FVTerm (FVTerm&& fvterm) noexcept  // move constructor
{
  foutput = std::shared_ptr<FOutput>(fvterm.foutput);
  window_list = std::shared_ptr<FVTermList>(fvterm.window_list);
}

//----------------------------------------------------------------------
FVTerm::~FVTerm()  // destructor
{
  if ( init_object == this )
    finish();
}


// Overloaded operators
//----------------------------------------------------------------------
FVTerm& FVTerm::operator = (const FVTerm& fvterm)  // copy assignment operator (=)
{
  foutput = std::shared_ptr<FOutput>(fvterm.foutput);
  window_list = std::shared_ptr<FVTermList>(fvterm.window_list);
  return *this;
}

//----------------------------------------------------------------------
FVTerm& FVTerm::operator = (FVTerm&& fvterm) noexcept  // move assignment operator (=)
{
  foutput = std::shared_ptr<FOutput>(fvterm.foutput);
  window_list = std::shared_ptr<FVTermList>(fvterm.window_list);
  return *this;
}

//----------------------------------------------------------------------
FVTerm& FVTerm::operator << (const FTermBuffer& term_buffer)
{
  print (term_buffer);
  return *this;
}

// public methods of FVTerm
//----------------------------------------------------------------------
auto FVTerm::getFOutput() -> std::shared_ptr<FOutput>
{
  return init_object->foutput;
}

//----------------------------------------------------------------------
FPoint FVTerm::getPrintCursor()
{
  const auto& win = getPrintArea();

  if ( win )
    return { win->offset_left + win->cursor_x
           , win->offset_top + win->cursor_y };

  return {0, 0};
}

//----------------------------------------------------------------------
void FVTerm::setTerminalUpdates (TerminalUpdate refresh_state) const
{
  if ( refresh_state == TerminalUpdate::Stop )
  {
    no_terminal_updates = true;
  }
  else if ( refresh_state == TerminalUpdate::Continue
         || refresh_state == TerminalUpdate::Start )
  {
    no_terminal_updates = false;
  }

  if ( refresh_state == TerminalUpdate::Start )
    updateTerminal();
}

//----------------------------------------------------------------------
void FVTerm::setCursor (const FPoint& pos)
{
  if ( auto win = getPrintArea() )
  {
    win->cursor_x = pos.getX() - win->offset_left;
    win->cursor_y = pos.getY() - win->offset_top;
  }
}

//----------------------------------------------------------------------
void FVTerm::setNonBlockingRead (bool enable)
{
  init_object->foutput->setNonBlockingRead (enable);
}

//----------------------------------------------------------------------
void FVTerm::clearArea (wchar_t fillchar)
{
  clearArea (vwin, fillchar);
}

//----------------------------------------------------------------------
void FVTerm::createVTerm (const FSize& size)
{
  // initialize virtual terminal

  const FRect box{0, 0, size.getWidth(), size.getHeight()};
  const FSize shadow{0, 0};
  createArea (box, shadow, vterm);
}

//----------------------------------------------------------------------
void FVTerm::resizeVTerm (const FSize& size) const
{
  // resize virtual terminal

  const FRect box{0, 0, size.getWidth(), size.getHeight()};
  const FSize shadow{0, 0};
  resizeArea (box, shadow, vterm);
}

//----------------------------------------------------------------------
void FVTerm::putVTerm() const
{
  for (auto i{0}; i < vterm->height; i++)
  {
    vterm->changes[i].xmin = 0;
    vterm->changes[i].xmax = uInt(vterm->width - 1);
  }

  updateTerminal();
}

//----------------------------------------------------------------------
bool FVTerm::updateTerminal() const
{
  return foutput->updateTerminal();
}

//----------------------------------------------------------------------
void FVTerm::addPreprocessingHandler ( const FVTerm* instance
                                     , FPreprocessingFunction&& function )
{
  if ( ! print_area )
    FVTerm::getPrintArea();

  if ( print_area )
  {
    delPreprocessingHandler (instance);
    auto obj = make_unique<FVTermPreprocessing> \
        (instance, std::move(function));
    print_area->preproc_list.emplace_back(std::move(obj));
  }
}

//----------------------------------------------------------------------
void FVTerm::delPreprocessingHandler (const FVTerm* instance)
{
  if ( ! print_area )
    FVTerm::getPrintArea();

  if ( ! print_area || print_area->preproc_list.empty() )
    return;

  auto iter = print_area->preproc_list.begin();

  while ( iter != print_area->preproc_list.end() )
  {
    if ( iter->get()->instance.get() == instance )
      iter = print_area->preproc_list.erase(iter);
    else
      ++iter;
  }
}

//----------------------------------------------------------------------
int FVTerm::print (const FString& string)
{
  if ( string.isEmpty() )
    return 0;

  FTermBuffer term_buffer{};
  term_buffer.write(string);
  return print (term_buffer);
}

//----------------------------------------------------------------------
int FVTerm::print (FTermArea* area, const FString& string)
{
  if ( ! area || string.isEmpty() )
    return -1;

  FTermBuffer term_buffer{};
  term_buffer.write(string);
  return print (area, term_buffer);
}

//----------------------------------------------------------------------
int FVTerm::print (const std::vector<FChar>& term_string)
{
  if ( term_string.empty() )
    return -1;

  FTermBuffer term_buffer{term_string.begin(), term_string.end()};
  return print (term_buffer);
}

//----------------------------------------------------------------------
int FVTerm::print (FTermArea* area, const std::vector<FChar>& term_string)
{
  if ( ! area || term_string.empty() )
    return -1;

  FTermBuffer term_buffer{term_string.begin(), term_string.end()};
  return print (area, term_buffer);
}

//----------------------------------------------------------------------
int FVTerm::print (const FTermBuffer& term_buffer)
{
  if ( term_buffer.isEmpty() )
    return -1;

  auto area = getPrintArea();

  if ( ! area )
    return -1;

  return print (area, term_buffer);
}

//----------------------------------------------------------------------
int FVTerm::print (FTermArea* area, const FTermBuffer& term_buffer)
{
  int len{0};
  const auto tabstop = uInt(foutput->getTabstop());

  if ( ! area || term_buffer.isEmpty() )
    return -1;

  for (auto&& fchar : term_buffer)
  {
    bool printable_character{false};

    switch ( fchar.ch[0] )
    {
      case '\n':
        area->cursor_y++;
        // fall through
      case '\r':
        area->cursor_x = 1;
        break;

      case '\t':
        area->cursor_x = int ( uInt(area->cursor_x)
                             + tabstop
                             - uInt(area->cursor_x)
                             + 1
                             % tabstop );
        break;

      case '\b':
        area->cursor_x--;
        break;

      case '\a':
        foutput->beep();
        break;

      default:
        print (area, fchar);  // print next character
        printable_character = true;
    }

    if ( ! printable_character && printWrap(area) )
      break;  // end of area reached

    len++;
  }

  return len;
}

//----------------------------------------------------------------------
int FVTerm::print (wchar_t c)
{
  FChar nc{FVTerm::getAttribute()};  // next character
  nc.ch[0] = c;
  nc.attr.byte[2] = 0;
  nc.attr.byte[3] = 0;
  return print (nc);
}

//----------------------------------------------------------------------
int FVTerm::print (FTermArea* area, wchar_t c)
{
  if ( ! area )
    return -1;

  FChar nc = FVTerm::getAttribute();  // next character
  nc.ch[0] = c;
  nc.attr.byte[2] = 0;
  nc.attr.byte[3] = 0;
  return print (area, nc);
}

//----------------------------------------------------------------------
int FVTerm::print (FChar& term_char)
{
  auto area = getPrintArea();

  if ( ! area )
    return -1;

  return print (area, term_char);
}

//----------------------------------------------------------------------
int FVTerm::print (FTermArea* area, const FChar& term_char)
{
  auto fchar = term_char;
  return print (area, fchar);
}

//----------------------------------------------------------------------
int FVTerm::print (FTermArea* area, FChar& term_char)
{
  if ( ! area )
    return -1;

  const int ax = area->cursor_x - 1;
  const int ay = area->cursor_y - 1;

  if ( term_char.attr.bit.char_width == 0 )
    addColumnWidth(term_char);  // add column width

  auto char_width = term_char.attr.bit.char_width;

  if ( char_width == 0 && ! term_char.attr.bit.fullwidth_padding )
    return 0;

  // Print term_char on area at position (ax, ay)
  printCharacterOnCoordinate (area, ax, ay, term_char);
  area->cursor_x++;
  area->has_changes = true;

  // Line break at right margin
  if ( area->cursor_x > getFullAreaWidth(area) )
  {
    area->cursor_x = 1;
    area->cursor_y++;
  }
  else if ( char_width == 2 )
    printPaddingCharacter (area, term_char);

  // Prevent up scrolling
  if ( area->cursor_y > getFullAreaHeight(area) )
  {
    area->cursor_y--;
    return -1;
  }

  return 1;
}

//----------------------------------------------------------------------
void FVTerm::print (const FPoint& p)
{
  setCursor (p);
}

//----------------------------------------------------------------------
void FVTerm::print (const FStyle& style)
{
  Style attr = style.getStyle();

  if ( attr == Style::None )
    setNormal();
  else
  {
    if ( (attr & Style::Bold) != Style::None ) setBold();
    if ( (attr & Style::Dim) != Style::None ) setDim();
    if ( (attr & Style::Italic) != Style::None ) setItalic();
    if ( (attr & Style::Underline) != Style::None ) setUnderline();
    if ( (attr & Style::Blink) != Style::None ) setBlink();
    if ( (attr & Style::Reverse) != Style::None ) setReverse();
    if ( (attr & Style::Standout) != Style::None ) setStandout();
    if ( (attr & Style::Invisible) != Style::None ) setInvisible();
    if ( (attr & Style::Protected) != Style::None ) setProtected();
    if ( (attr & Style::CrossedOut) != Style::None ) setCrossedOut();
    if ( (attr & Style::DoubleUnderline) != Style::None ) setDoubleUnderline();
    if ( (attr & Style::Transparent) != Style::None ) setTransparent();
    if ( (attr & Style::ColorOverlay) != Style::None ) setColorOverlay();
    if ( (attr & Style::InheritBackground) != Style::None ) setInheritBackground();
  }
}

//----------------------------------------------------------------------
void FVTerm::print (const FColorPair& pair)
{
  setColor (pair.getForegroundColor(), pair.getBackgroundColor());
}

//----------------------------------------------------------------------
void FVTerm::flush() const
{
  foutput->flush();
}


// protected methods of FVTerm
//----------------------------------------------------------------------
FVTerm::FTermArea* FVTerm::getPrintArea()
{
  // returns the print area of this object

  if ( print_area )
    return print_area;
  else
  {
    if ( vwin )
    {
      print_area = vwin;
      return print_area;
    }
    else if ( child_print_area )
    {
      print_area = child_print_area;
      return print_area;
    }
  }

  return vdesktop;
}

//----------------------------------------------------------------------
void FVTerm::createArea ( const FRect& box
                        , const FSize& shadow
                        , FTermArea*& area )
{
  // initialize virtual window

  try
  {
    area = new FTermArea;
  }
  catch (const std::bad_alloc&)
  {
    badAllocOutput ("FTermArea");
    return;
  }

  area->setOwner<FVTerm*>(this);
  resizeArea (box, shadow, area);
}

//----------------------------------------------------------------------
void FVTerm::resizeArea ( const FRect& box
                        , const FSize& shadow
                        , FTermArea* area ) const
{
  // Resize the virtual window to a new size.

  const int offset_left = box.getX();
  const int offset_top  = box.getY();
  const auto width = int(box.getWidth());
  const auto height = int(box.getHeight());
  const auto rsw = int(shadow.getWidth());
  const auto bsh = int(shadow.getHeight());

  assert ( offset_top >= 0 );
  assert ( width > 0 && width + rsw > 0 );
  assert ( height > 0 && height + bsh > 0 );
  assert ( rsw >= 0 );
  assert ( bsh >= 0 );

  if ( ! area )
    return;

  if ( width == area->width
    && height == area->height
    && rsw == area->right_shadow
    && bsh == area->bottom_shadow )
  {
    if ( offset_left != area->offset_left )
      area->offset_left = offset_left;

    if ( offset_top != area->offset_top )
      area->offset_top = offset_top;

    return;
  }

  bool realloc_success{false};
  const std::size_t full_width = std::size_t(width) + std::size_t(rsw);
  const std::size_t full_height = std::size_t(height) + std::size_t(bsh);
  const std::size_t area_size = full_width * full_height;

  if ( getFullAreaHeight(area) != int(full_height) )
  {
    realloc_success = reallocateTextArea ( area
                                         , full_height
                                         , area_size );
  }
  else if ( getFullAreaWidth(area) != int(full_width) )
  {
    realloc_success = reallocateTextArea (area, area_size);
  }
  else
    return;

  if ( ! realloc_success )
    return;

  area->offset_left   = offset_left;
  area->offset_top    = offset_top;
  area->width         = width;
  area->height        = height;
  area->min_width     = width;
  area->min_height    = DEFAULT_MINIMIZED_HEIGHT;
  area->right_shadow  = rsw;
  area->bottom_shadow = bsh;
  area->has_changes   = false;

  const FSize size{full_width, full_height};
  resetTextAreaToDefault (area, size);
}

//----------------------------------------------------------------------
void FVTerm::removeArea (FTermArea*& area)
{
  // remove the virtual window

  if ( area == nullptr )
    return;

  if ( area->changes != nullptr )
  {
    delete[] area->changes;
    area->changes = nullptr;
  }

  if ( area->data != nullptr )
  {
    delete[] area->data;
    area->data = nullptr;
  }

  delete area;
  area = nullptr;
}

//----------------------------------------------------------------------
void FVTerm::restoreVTerm (const FRect& box)
{
  if ( ! vterm )
    return;

  int x = box.getX() - 1;
  int y = box.getY() - 1;
  auto w = int(box.getWidth());
  auto h = int(box.getHeight());

  if ( x < 0 )
    x = 0;

  if ( y < 0 )
    y = 0;

  if ( x + w > vterm->width )
    w = vterm->width - x;

  if ( y + h > vterm->height )
    h = vterm->height - y;

  if ( w < 0 || h < 0 )
    return;

  for (auto ty{0}; ty < h; ty++)
  {
    const int ypos = y + ty;

    for (auto tx{0}; tx < w; tx++)
    {
      const int xpos = x + tx;
      auto& tc = vterm->data[ypos * vterm->width + xpos];  // terminal character
      auto sc = generateCharacter(FPoint{xpos, ypos});  // shown character
      std::memcpy (&tc, &sc, sizeof(tc));
    }

    if ( int(vterm->changes[ypos].xmin) > x )
      vterm->changes[ypos].xmin = uInt(x);

    if ( int(vterm->changes[ypos].xmax) < x + w - 1 )
      vterm->changes[ypos].xmax = uInt(x + w - 1);
  }

  vterm->has_changes = true;
}

//----------------------------------------------------------------------
bool FVTerm::updateVTermCursor (const FTermArea* area) const
{
  if ( ! (area && isActive(area) && area->visible) )
    return false;

  if ( area->input_cursor_visible )
  {
    // area cursor position
    const int cx = area->input_cursor_x;
    const int cy = area->input_cursor_y;
    // terminal position = area offset + area cursor position
    const int x  = area->offset_left + cx;
    const int y  = area->offset_top + cy;

    if ( isInsideArea (FPoint{cx, cy}, area)
      && isInsideTerminal (FPoint{x, y})
      && isCovered (FPoint{x, y}, area) == CoveredState::None )
    {
      vterm->input_cursor_x = x;
      vterm->input_cursor_y = y;
      vterm->input_cursor_visible = true;
      vterm->has_changes = true;
      return true;
    }
  }

  vterm->input_cursor_visible = false;
  return false;
}

//----------------------------------------------------------------------
bool FVTerm::isCursorHideable() const
{
  return foutput->isCursorHideable();
}

//----------------------------------------------------------------------
void FVTerm::setAreaCursor ( const FPoint& pos
                           , bool visible
                           , FTermArea* area )
{
  if ( ! area )
    return;

  area->input_cursor_x = pos.getX() - 1;
  area->input_cursor_y = pos.getY() - 1;
  area->input_cursor_visible = visible;
}

//----------------------------------------------------------------------
void FVTerm::getArea (const FPoint& pos, const FTermArea* area)
{
  // Copies a block from the virtual terminal position to the given area

  if ( ! area )
    return;

  const int ax = pos.getX() - 1;
  const int ay = pos.getY() - 1;
  int y_end{};
  int length{};

  if ( area->height + ay > vterm->height )
    y_end = area->height - ay;
  else
    y_end = area->height;

  if ( area->width + ax > vterm->width )
    length = vterm->width - ax;
  else
    length = area->width;

  for (auto y{0}; y < y_end; y++)  // line loop
  {
    const auto& tc = vterm->data[(ay + y) * vterm->width + ax];  // terminal character
    auto& ac = area->data[y * area->width];  // area character
    std::memcpy (&ac, &tc, sizeof(ac) * unsigned(length));

    if ( int(area->changes[y].xmin) > 0 )
      area->changes[y].xmin = 0;

    if ( int(area->changes[y].xmax) < length - 1 )
      area->changes[y].xmax = uInt(length - 1);
  }
}

//----------------------------------------------------------------------
void FVTerm::getArea (const FRect& box, const FTermArea* area)
{
  // Copies a block from the virtual terminal rectangle to the given area

  if ( ! area )
    return;

  const int x = box.getX();
  const int y = box.getY();
  const auto w = int(box.getWidth());
  const auto h = int(box.getHeight());
  const int dx = x - area->offset_left + 1;
  const int dy = y - area->offset_top + 1;
  int y_end{};
  int length{};

  if ( x < 0 || y < 0 )
    return;

  if ( y - 1 + h > vterm->height )
    y_end = vterm->height - y + 1;
  else
    y_end = h - 1;

  if ( x - 1 + w > vterm->width )
    length = vterm->width - x + 1;
  else
    length = w;

  if ( length < 1 )
    return;

  for (auto _y = 0; _y < y_end; _y++)  // line loop
  {
    const int line_len = getFullAreaWidth(area);
    const auto& tc = vterm->data[(y + _y - 1) * vterm->width + x - 1];  // terminal character
    auto& ac = area->data[(dy + _y) * line_len + dx];  // area character
    std::memcpy (&ac, &tc, sizeof(ac) * unsigned(length));

    if ( int(area->changes[dy + _y].xmin) > dx )
      area->changes[dy + _y].xmin = uInt(dx);

    if ( int(area->changes[dy + _y].xmax) < dx + length - 1 )
      area->changes[dy + _y].xmax = uInt(dx + length - 1);
  }
}

//----------------------------------------------------------------------
void FVTerm::putArea (const FTermArea* area) const
{
  // Add area changes to the virtual terminal

  if ( ! area || ! area->visible )
    return;

  int ax  = area->offset_left;
  const int ay  = area->offset_top;
  const int width = getFullAreaWidth(area);
  const int height = area->minimized ? area->min_height : getFullAreaHeight(area);
  int ol{0};  // Outside left
  int y_end{};

  // Call the preprocessing handler methods
  callPreprocessingHandler(area);

  if ( ax < 0 )
  {
    ol = std::abs(ax);
    ax = 0;
  }

  if ( height + ay > vterm->height )
    y_end = vterm->height - ay;
  else
    y_end = height;

  for (auto y{0}; y < y_end; y++)  // Line loop
  {
    bool modified{false};
    auto line_xmin = int(area->changes[y].xmin);
    auto line_xmax = int(area->changes[y].xmax);

    if ( line_xmin > line_xmax )
      continue;

    if ( ax == 0 )
      line_xmin = ol;

    if ( width + ax - ol >= vterm->width )
      line_xmax = vterm->width + ol - ax - 1;

    if ( ax + line_xmin >= vterm->width )
      continue;

    for (auto x = line_xmin; x <= line_xmax; x++)  // Column loop
    {
      // Global terminal positions
      int tx = ax + x;
      const int ty = ay + y;

      if ( tx < 0 || ty < 0 )
        continue;

      tx -= ol;
      bool update = updateVTermCharacter(area, FPoint{x, y}, FPoint{tx, ty});

      if ( ! modified && ! update )
        line_xmin++;  // Don't update covered character

      if ( update )
        modified = true;
    }

    int _xmin = ax + line_xmin - ol;
    int _xmax = ax + line_xmax;

    if ( _xmin < int(vterm->changes[ay + y].xmin) )
      vterm->changes[ay + y].xmin = uInt(_xmin);

    if ( _xmax >= vterm->width )
      _xmax = vterm->width - 1;

    if ( _xmax > int(vterm->changes[ay + y].xmax) )
      vterm->changes[ay + y].xmax = uInt(_xmax);

    area->changes[y].xmin = uInt(width);
    area->changes[y].xmax = 0;
  }

  vterm->has_changes = true;
  updateVTermCursor(area);
}

//----------------------------------------------------------------------
void FVTerm::putArea (const FPoint& pos, const FTermArea* area)
{
  // Copies the given area block to the virtual terminal position

  if ( ! area || ! area->visible )
    return;

  int ax = pos.getX() - 1;
  const int ay = pos.getY() - 1;
  const int width = getFullAreaWidth(area);
  const int height = area->minimized ? area->min_height : getFullAreaHeight(area);
  int ol{0};  // outside left
  int y_end{};
  int length{};

  if ( ax < 0 )
  {
    ol = std::abs(ax);
    ax = 0;
  }

  if ( ay + height > vterm->height )
    y_end = vterm->height - ay;
  else
    y_end = height;

  if ( width - ol + ax > vterm->width )
    length = vterm->width - ax;
  else
    length = width - ol;

  if ( length < 1 )
    return;

  for (auto y{0}; y < y_end; y++)  // line loop
  {
    if ( area->changes[y].trans_count == 0 )
    {
      // Line has only covered characters
      const auto& ac = area->data[y * width + ol];           // area character
      auto& tc = vterm->data[(ay + y) * vterm->width + ax];  // terminal character
      putAreaLine (ac, tc, std::size_t(length));
    }
    else
    {
      // Line has one or more transparent characters
      for (auto x{0}; x < length; x++)  // column loop
      {
        const int cx = ax + x;
        const int cy = ay + y;
        const auto& ac = area->data[y * width + ol + x];  // area character
        auto& tc = vterm->data[cy * vterm->width + cx];   // terminal character
        putAreaCharacter (FPoint{cx, cy}, area, ac, tc);
      }
    }

    if ( ax < int(vterm->changes[ay + y].xmin) )
      vterm->changes[ay + y].xmin = uInt(ax);

    if ( ax + length - 1 > int(vterm->changes[ay + y].xmax) )
      vterm->changes[ay + y].xmax = uInt(ax + length - 1);
  }

  vterm->has_changes = true;
}

//----------------------------------------------------------------------
int FVTerm::getLayer (const FVTerm* obj)
{
  // returns the layer from the FVTerm object

  if ( ! getWindowList() || getWindowList()->empty() )
    return -1;

  auto iter = getWindowList()->begin();
  const auto end = getWindowList()->end();

  while ( iter != end )
  {
    if ( *iter == obj )
      break;

    ++iter;
  }

  return int(std::distance(getWindowList()->begin(), iter) + 1);
}

//----------------------------------------------------------------------
void FVTerm::scrollAreaForward (FTermArea* area) const
{
  // Scrolls the entire area on line up

  if ( ! area || area->height <= 1 )
    return;

  const int length = area->width;
  const int total_width = getFullAreaWidth(area);
  const int y_max = area->height - 1;

  for (auto y{0}; y < y_max; y++)
  {
    const int pos1 = y * total_width;
    const int pos2 = (y + 1) * total_width;
    const auto& sc = area->data[pos2];  // source character
    auto& dc = area->data[pos1];  // destination character
    std::memcpy (&dc, &sc, sizeof(dc) * unsigned(length));
    area->changes[y].xmin = 0;
    area->changes[y].xmax = uInt(area->width - 1);
  }

  // insert a new line below
  FChar nc{};  // next character
  auto bottom_right = std::size_t((y_max * total_width) - area->right_shadow - 1);
  const auto& lc = area->data[bottom_right];  // last character
  std::memcpy (&nc, &lc, sizeof(nc));
  nc.ch[0] = L' ';
  auto& dc = area->data[y_max * total_width];  // destination character
  std::fill_n (&dc, area->width, nc);
  area->changes[y_max].xmin = 0;
  area->changes[y_max].xmax = uInt(area->width - 1);
  area->has_changes = true;

  // Scrolls the terminal up one line
  foutput->scrollAreaForward(area);
}

//----------------------------------------------------------------------
void FVTerm::scrollAreaReverse (FTermArea* area) const
{
  // Scrolls the entire area one line down

  if ( ! area || area->height <= 1 )
    return;

  const int length = area->width;
  const int total_width = getFullAreaWidth(area);
  const int y_max = area->height - 1;

  for (auto y = y_max; y > 0; y--)
  {
    const int pos1 = (y - 1) * total_width;
    const int pos2 = y * total_width;
    const auto& sc = area->data[pos1];  // source character
    auto& dc = area->data[pos2];  // destination character
    std::memcpy (&dc, &sc, sizeof(dc) * unsigned(length));
    area->changes[y].xmin = 0;
    area->changes[y].xmax = uInt(area->width - 1);
  }

  // insert a new line above
  FChar nc{};  // next character
  const auto& lc = area->data[total_width];  // last character
  std::memcpy (&nc, &lc, sizeof(nc));
  nc.ch[0] = L' ';
  auto& dc = area->data[0];  // destination character
  std::fill_n (&dc, area->width, nc);
  area->changes[0].xmin = 0;
  area->changes[0].xmax = uInt(area->width - 1);
  area->has_changes = true;

  // Scrolls the terminal down one line
  foutput->scrollAreaReverse(area);
}

//----------------------------------------------------------------------
void FVTerm::clearArea (FTermArea* area, wchar_t fillchar) const
{
  // Clear the area with the current attributes

  FChar nc{};  // next character

  // Current attributes with a space character
  std::memcpy (&nc, &next_attribute, sizeof(nc));
  nc.ch[0] = fillchar;

  if ( ! (area && area->data) )
  {
    foutput->clearTerm (fillchar);
    return;
  }

  const auto w = uInt(getFullAreaWidth(area));

  if ( area->right_shadow == 0 )
  {
    if ( clearFullArea(area, nc) )
      return;
  }
  else
    clearAreaWithShadow(area, nc);

  for (auto i{0}; i < area->height; i++)
  {
    area->changes[i].xmin = 0;
    area->changes[i].xmax = w - 1;

    if ( nc.attr.bit.transparent
      || nc.attr.bit.color_overlay
      || nc.attr.bit.inherit_background )
      area->changes[i].trans_count = w;
    else if ( area->right_shadow != 0 )
      area->changes[i].trans_count = uInt(area->right_shadow);
    else
      area->changes[i].trans_count = 0;
  }

  for (auto i{0}; i < area->bottom_shadow; i++)
  {
    const int y = area->height + i;
    area->changes[y].xmin = 0;
    area->changes[y].xmax = w - 1;
    area->changes[y].trans_count = w;
  }

  area->has_changes = true;
}

//----------------------------------------------------------------------
void FVTerm::forceTerminalUpdate() const
{
  force_terminal_update = true;
  processTerminalUpdate();
  flush();
  force_terminal_update = false;
}

//----------------------------------------------------------------------
bool FVTerm::processTerminalUpdate() const
{
  // Checks if the resizing of the terminal is not finished
  if ( FVTerm::getFOutput()->hasTerminalResized() )
    return false;

  // Update data on VTerm
  updateVTerm();

  // Update the visible terminal
  return updateTerminal();
}

//----------------------------------------------------------------------
void FVTerm::startDrawing()
{
  // Pauses the terminal updates for the printing phase
  draw_completed = false;
}

//----------------------------------------------------------------------
void FVTerm::finishDrawing()
{
  // After the printing phase is completed, the terminal will be updated
  draw_completed = true;
}

//----------------------------------------------------------------------
void FVTerm::initTerminal()
{
  foutput->initTerminal();
}


// private methods of FVTerm
//----------------------------------------------------------------------
inline void FVTerm::resetTextAreaToDefault ( const FTermArea* area
                                           , const FSize& size ) const
{
  FChar default_char;
  FLineChanges unchanged;

  default_char.ch[0]        = L' ';
  default_char.fg_color     = FColor::Default;
  default_char.bg_color     = FColor::Default;
  default_char.attr.byte[0] = 0;
  default_char.attr.byte[1] = 0;
  default_char.attr.byte[2] = 0;
  default_char.attr.byte[3] = 0;

  std::fill_n (area->data, size.getArea(), default_char);

  unchanged.xmin = uInt(size.getWidth());
  unchanged.xmax = 0;
  unchanged.trans_count = 0;

  std::fill_n (area->changes, size.getHeight(), unchanged);
}

//----------------------------------------------------------------------
inline bool FVTerm::reallocateTextArea ( FTermArea* area
                                       , std::size_t height
                                       , std::size_t size )
{
  // Reallocate "height" lines for changes
  // and "size" bytes for the text area

  if ( area->changes != nullptr )
    delete[] area->changes;

  if ( area->data != nullptr )
    delete[] area->data;

  try
  {
    area->changes = new FLineChanges[height];
    area->data    = new FChar[size];
  }
  catch (const std::bad_alloc&)
  {
    badAllocOutput ("FLineChanges[height] or FChar[size]");
    return false;
  }

  return true;
}

//----------------------------------------------------------------------
inline bool FVTerm::reallocateTextArea (FTermArea* area, std::size_t size)
{
  // Reallocate "size" bytes for the text area

  if ( area->data != nullptr )
    delete[] area->data;

  try
  {
    area->data = new FChar[size];
  }
  catch (const std::bad_alloc&)
  {
    badAllocOutput ("FChar[size]");
    return false;
  }

  return true;
}

//----------------------------------------------------------------------
FVTerm::CoveredState FVTerm::isCovered ( const FPoint& pos
                                       , const FTermArea* area )
{
  // Determines the covered state for the given position

  if ( ! area )
    return CoveredState::None;

  auto is_covered = CoveredState::None;

  if ( getWindowList() && ! getWindowList()->empty() )
  {
    bool found{ area == vdesktop };

    for (auto& win_obj : *getWindowList())
    {
      const auto& win = win_obj->getVWin();

      if ( ! (win && win->visible) )
        continue;

      const int& win_x = win->offset_left;
      const int& win_y = win->offset_top;
      const int height = win->minimized ? win->min_height : getFullAreaHeight(win);
      const FRect geometry { win_x, win_y
                           , std::size_t(getFullAreaWidth(win))
                           , std::size_t(height) };

      if ( found && geometry.contains(pos) )
      {
        const int width = getFullAreaWidth(win);
        const int& x = pos.getX();
        const int& y = pos.getY();
        const auto& tmp = &win->data[(y - win_y) * width + (x - win_x)];

        if ( tmp->attr.bit.color_overlay )
        {
          is_covered = CoveredState::Half;
        }
        else if ( ! tmp->attr.bit.transparent )
        {
          is_covered = CoveredState::Full;
          break;
        }
      }

      if ( area == win )
        found = true;
    }
  }

  return is_covered;
}

//----------------------------------------------------------------------
constexpr int FVTerm::getFullAreaWidth (const FTermArea* area)
{
  return area->width + area->right_shadow;
}

//----------------------------------------------------------------------
constexpr int FVTerm::getFullAreaHeight (const FTermArea* area)
{
  return area->height + area->bottom_shadow;
}

//----------------------------------------------------------------------
inline void FVTerm::updateOverlappedColor ( const FChar& area_char
                                          , const FChar& over_char
                                          , FChar& vterm_char )
{
  // Add the overlapping color to this character

  // New character
  FChar nc{};
  std::memcpy (&nc, &area_char, sizeof(nc));
  nc.fg_color = over_char.fg_color;
  nc.bg_color = over_char.bg_color;
  nc.attr.bit.reverse  = false;
  nc.attr.bit.standout = false;

  if ( isTransparentInvisible(nc) )
    nc.ch[0] = L' ';

  nc.attr.bit.no_changes = bool(vterm_char.attr.bit.printed && vterm_char == nc);
  std::memcpy (&vterm_char, &nc, sizeof(vterm_char));
}

//----------------------------------------------------------------------
inline void FVTerm::updateOverlappedCharacter ( FChar& cover_char
                                              , FChar& vterm_char )
{
  // Restore one character on vterm

  cover_char.attr.bit.no_changes = \
      bool(vterm_char.attr.bit.printed && vterm_char == cover_char);
  std::memcpy (&vterm_char, &cover_char, sizeof(vterm_char));
}

//----------------------------------------------------------------------
inline void FVTerm::updateShadedCharacter ( const FChar& area_char
                                          , FChar& cover_char
                                          , FChar& vterm_char )
{
  // Get covered character + add the current color

  cover_char.fg_color = area_char.fg_color;
  cover_char.bg_color = area_char.bg_color;
  cover_char.attr.bit.reverse  = false;
  cover_char.attr.bit.standout = false;

  if ( isTransparentInvisible(cover_char) )
    cover_char.ch[0] = L' ';

  cover_char.attr.bit.no_changes = \
      bool(vterm_char.attr.bit.printed && vterm_char == cover_char);
  std::memcpy (&vterm_char, &cover_char, sizeof(vterm_char));
}

//----------------------------------------------------------------------
inline void FVTerm::updateInheritBackground ( const FChar& area_char
                                            , const FChar& cover_char
                                            , FChar& vterm_char )
{
  // Add the covered background to this character

  // New character
  FChar nc{};
  std::memcpy (&nc, &area_char, sizeof(nc));
  nc.bg_color = cover_char.bg_color;
  nc.attr.bit.no_changes = \
      bool(vterm_char.attr.bit.printed && vterm_char == nc);
  std::memcpy (&vterm_char, &nc, sizeof(vterm_char));
}

//----------------------------------------------------------------------
inline void FVTerm::updateCharacter (const FChar& area_char, FChar& vterm_char)
{
  // Copy a area character to the virtual terminal

  std::memcpy (&vterm_char, &area_char, sizeof(vterm_char));

  if ( vterm_char.attr.bit.printed && vterm_char == area_char )
    vterm_char.attr.bit.no_changes = true;
  else
    vterm_char.attr.bit.no_changes = false;
}

//----------------------------------------------------------------------
bool FVTerm::updateVTermCharacter ( const FTermArea* area
                                  , const FPoint& area_pos
                                  , const FPoint& terminal_pos )
{
  // Area character
  const int width = getFullAreaWidth(area);
  const int area_index = area_pos.getY() * width + area_pos.getX();
  const auto& ac = area->data[area_index];
  // Terminal character
  const int terminal_index = terminal_pos.getY() * vterm->width
                           + terminal_pos.getX();
  auto& tc = vterm->data[terminal_index];

  // Get covered state
  const auto is_covered = isCovered(terminal_pos, area);

  if ( is_covered == CoveredState::Full )
    return false;

  if ( is_covered == CoveredState::Half )
  {
    // Overlapped character
    auto oc = getOverlappedCharacter (terminal_pos, area);
    updateOverlappedColor (ac, oc, tc);
  }
  else if ( ac.attr.bit.transparent )   // Transparent
  {
    // Covered character
    auto cc = getCoveredCharacter (terminal_pos, area);
    updateOverlappedCharacter (cc, tc);
  }
  else  // Not transparent
  {
    if ( ac.attr.bit.color_overlay )  // Transparent shadow
    {
      // Covered character
      auto cc = getCoveredCharacter (terminal_pos, area);
      updateShadedCharacter (ac, cc, tc);
    }
    else if ( ac.attr.bit.inherit_background )
    {
      // Covered character
      auto cc = getCoveredCharacter (terminal_pos, area);
      updateInheritBackground (ac, cc, tc);
    }
    else  // Default
    {
      updateCharacter (ac, tc);
    }
  }

  return true;
}

//----------------------------------------------------------------------
void FVTerm::updateVTerm() const
{
  // Updates the character data from all areas to VTerm

  if ( hasPendingUpdates(vdesktop) )
  {
    putArea(vdesktop);
    vdesktop->has_changes = false;
  }

  if ( ! getWindowList() || getWindowList()->empty() )
    return;

  for (auto&& window : *getWindowList())
  {
    auto v_win = window->getVWin();

    if ( ! (v_win && v_win->visible) )
      continue;

    if ( hasPendingUpdates(v_win) )
    {
      putArea(v_win);
      v_win->has_changes = false;
    }
    else if ( hasChildAreaChanges(v_win) )
    {
      putArea(v_win);  // and call the child area processing handler there
      clearChildAreaChanges(v_win);
    }
  }
}

//----------------------------------------------------------------------
void FVTerm::callPreprocessingHandler (const FTermArea* area)
{
  // Call preprocessing handler

  if ( ! area || area->preproc_list.empty() )
    return;

  for (auto&& pcall : area->preproc_list)
  {
    // call the preprocessing handler
    auto preprocessingHandler = pcall->function;
    preprocessingHandler();
  }
}

//----------------------------------------------------------------------
bool FVTerm::hasChildAreaChanges (FTermArea* area) const
{
  if ( ! area || area->preproc_list.empty() )
    return false;

  return std::any_of ( area->preproc_list.begin()
                     , area->preproc_list.end()
                     , [] (const std::unique_ptr<FVTermPreprocessing>& pcall)
                       {
                         return pcall->instance
                             && pcall->instance->child_print_area
                             && pcall->instance->child_print_area->has_changes;
                       }
                     );
}

//----------------------------------------------------------------------
void FVTerm::clearChildAreaChanges (const FTermArea* area) const
{
  if ( ! area || area->preproc_list.empty() )
    return;

  for (auto&& pcall : area->preproc_list)
  {
    if ( pcall->instance && pcall->instance->child_print_area )
      pcall->instance->child_print_area->has_changes = false;
  }
}

//----------------------------------------------------------------------
bool FVTerm::isInsideArea (const FPoint& pos, const FTermArea* area)
{
  // Check whether the coordinates are within the area

  const auto aw = std::size_t(area->width);
  const auto ah = std::size_t(area->height);
  FRect area_geometry{0, 0, aw, ah};

  if ( area_geometry.contains(pos) )
    return true;
  else
    return false;
}

//----------------------------------------------------------------------
bool FVTerm::isTransparentInvisible (const FChar& fchar)
{
  return ( fchar.ch[0] == UniChar::LowerHalfBlock
        || fchar.ch[0] == UniChar::UpperHalfBlock
        || fchar.ch[0] == UniChar::LeftHalfBlock
        || fchar.ch[0] == UniChar::RightHalfBlock
        || fchar.ch[0] == UniChar::MediumShade
        || fchar.ch[0] == UniChar::FullBlock )
  ? true
  : false;
}

//----------------------------------------------------------------------
FChar FVTerm::generateCharacter (const FPoint& pos)
{
  // Generates characters for a given position considering all areas

  const int x = pos.getX();
  const int y = pos.getY();
  auto sc = &vdesktop->data[y * vdesktop->width + x];  // shown character

  if ( ! getWindowList() || getWindowList()->empty() )
    return *sc;

  for (auto& win_obj : *getWindowList())
  {
    const auto& win = win_obj->getVWin();

    if ( ! win || ! win->visible )
      continue;

    const int win_x = win->offset_left;
    const int win_y = win->offset_top;
    const int height = win->minimized ? win->min_height : getFullAreaHeight(win);
    const FRect geometry { win_x, win_y
                         , std::size_t(getFullAreaWidth(win))
                         , std::size_t(height) };

    // Window is visible and contains current character
    if ( geometry.contains(x, y) )
    {
      const auto line_len = int(geometry.getWidth());
      auto tmp = &win->data[(y - win_y) * line_len + (x - win_x)];

      if ( ! tmp->attr.bit.transparent )   // Current character not transparent
      {
        if ( tmp->attr.bit.color_overlay )  // Transparent shadow
        {
          // Keep the current vterm character
          if ( sc != &s_ch )
            std::memcpy (&s_ch, sc, sizeof(s_ch));

          s_ch.fg_color = tmp->fg_color;
          s_ch.bg_color = tmp->bg_color;
          s_ch.attr.bit.reverse  = false;
          s_ch.attr.bit.standout = false;

          if ( isTransparentInvisible(s_ch) )
            s_ch.ch[0] = L' ';

          sc = &s_ch;
        }
        else if ( tmp->attr.bit.inherit_background )
        {
          // Add the covered background to this character
          std::memcpy (&i_ch, tmp, sizeof(i_ch));
          i_ch.bg_color = sc->bg_color;  // Last background color
          sc = &i_ch;
        }
        else  // Default
          sc = tmp;
      }
    }
  }

  return *sc;
}

//----------------------------------------------------------------------
FChar FVTerm::getCharacter ( CharacterType char_type
                           , const FPoint& pos
                           , const FTermArea* area )
{
  // Gets the overlapped or the covered character for a given position

  const int x = pos.getX();
  const int y = pos.getY();
  int xx = std::max(x, 0);
  int yy = std::max(y, 0);

  if ( xx >= vterm->width )
    xx = vterm->width - 1;

  if ( yy >= vterm->height )
    yy = vterm->height - 1;

  auto cc = &vdesktop->data[yy * vdesktop->width + xx];  // covered character

  if ( ! area || ! getWindowList() || getWindowList()->empty() )
    return *cc;

  // Get the window layer of this widget object
  const auto has_an_owner = area->hasOwner();
  const auto area_owner = area->getOwner<FVTerm*>();
  const int layer = has_an_owner ? getLayer(area_owner) : 0;

  for (auto&& win_obj : *getWindowList())
  {
    bool significant_char{false};

    // char_type can be "overlapped_character"
    // or "covered_character"
    if ( char_type == CharacterType::Covered )
      significant_char = bool(layer >= getLayer(win_obj));
    else
      significant_char = bool(layer < getLayer(win_obj));

    if ( has_an_owner && area_owner != win_obj && significant_char )
    {
      const auto& win = win_obj->getVWin();

      if ( ! win || ! win->visible )
        continue;

      const int height = win->minimized ? win->min_height : getFullAreaHeight(win);
      const FRect geometry { win->offset_left, win->offset_top
                           , std::size_t(getFullAreaWidth(win))
                           , std::size_t(height) };

      // Window visible and contains current character
      if ( geometry.contains(x, y) )
        getAreaCharacter (FPoint{x, y}, win, cc);
    }
    else if ( char_type == CharacterType::Covered )
      break;
  }

  return *cc;
}

//----------------------------------------------------------------------
inline FChar FVTerm::getCoveredCharacter (const FPoint& pos, const FTermArea* area)
{
  // Gets the covered character for a given position
  return getCharacter (CharacterType::Covered, pos, area);
}

//----------------------------------------------------------------------
inline FChar FVTerm::getOverlappedCharacter (const FPoint& pos, const FTermArea* area)
{
  // Gets the overlapped character for a given position
  return getCharacter (CharacterType::Overlapped, pos, area);
}

//----------------------------------------------------------------------
void FVTerm::init()
{
  init_object = this;
  vterm       = nullptr;
  vdesktop    = nullptr;

  try
  {
    foutput       = std::make_shared<FTermOutput>(*this);
    window_list   = std::make_shared<FVTermList>();
  }
  catch (const std::bad_alloc&)
  {
    badAllocOutput ("FTermOutput, or FVTermList");
    return;
  }

  // Presetting of the current locale for full-width character support.
  // The final setting is made later in FTerm::init_locale().
  std::setlocale (LC_ALL, "");

  // next_attribute contains the state of the next printed character
  next_attribute.ch           = {{ L'\0' }};
  next_attribute.fg_color     = FColor::Default;
  next_attribute.bg_color     = FColor::Default;
  next_attribute.attr.byte[0] = 0;
  next_attribute.attr.byte[1] = 0;
  next_attribute.attr.byte[2] = 0;
  next_attribute.attr.byte[3] = 0;

  // Create virtual terminal
  FRect term_geometry {0, 0, foutput->getColumnNumber(), foutput->getLineNumber()};
  createVTerm (term_geometry.getSize());

  // Create virtual desktop area
  FSize shadow_size{0, 0};
  createArea (term_geometry, shadow_size, vdesktop);
  vdesktop->visible = true;
  active_area = vdesktop;
}

//----------------------------------------------------------------------
void FVTerm::finish() const
{
  // Resetting the terminal
  setNormal();
  foutput->finishTerminal();
  forceTerminalUpdate();

  // remove virtual terminal + virtual desktop area
  removeArea (vdesktop);
  removeArea (vterm);

  init_object = nullptr;
}

//----------------------------------------------------------------------
void FVTerm::putAreaLine (const FChar& area_char, FChar& vterm_char, std::size_t length)
{
  // copy "length" characters from area to terminal

  std::memcpy (&vterm_char, &area_char, sizeof(vterm_char) * length);
}

//----------------------------------------------------------------------
void FVTerm::putAreaCharacter ( const FPoint& pos, const FTermArea* area
                              , const FChar& area_char, FChar& vterm_char )
{
  if ( area_char.attr.bit.transparent )  // Transparent
  {
    // Restore one character on vterm
    FChar ch = getCoveredCharacter (pos, area);
    std::memcpy (&vterm_char, &ch, sizeof(vterm_char));
  }
  else  // Mot transparent
  {
    if ( area_char.attr.bit.color_overlay )  // Transparent shadow
    {
      // Get covered character + add the current color
      FChar ch = getCoveredCharacter (pos, area);
      ch.fg_color = area_char.fg_color;
      ch.bg_color = area_char.bg_color;
      ch.attr.bit.reverse  = false;
      ch.attr.bit.standout = false;

      if ( isTransparentInvisible(ch) )
        ch.ch[0] = L' ';

      std::memcpy (&vterm_char, &ch, sizeof(vterm_char));
    }
    else if ( area_char.attr.bit.inherit_background )
    {
      // Add the covered background to this character
      FChar ch{};
      std::memcpy (&ch, &area_char, sizeof(ch));
      FChar cc = getCoveredCharacter (pos, area);
      ch.bg_color = cc.bg_color;
      std::memcpy (&vterm_char, &ch, sizeof(vterm_char));
    }
    else  // Default
      std::memcpy (&vterm_char, &area_char, sizeof(vterm_char));
  }
}

//----------------------------------------------------------------------
void FVTerm::getAreaCharacter ( const FPoint& pos, const FTermArea* area
                              , FChar*& cc )
{
  const int area_x = area->offset_left;
  const int area_y = area->offset_top;
  const int line_len = getFullAreaWidth(area);
  const int x = pos.getX();
  const int y = pos.getY();
  auto& tmp = area->data[(y - area_y) * line_len + (x - area_x)];

  // Current character not transparent
  if ( ! tmp.attr.bit.transparent )
  {
    if ( tmp.attr.bit.color_overlay )  // transparent shadow
    {
      // Keep the current vterm character
      std::memcpy (&s_ch, cc, sizeof(s_ch));
      s_ch.fg_color = tmp.fg_color;
      s_ch.bg_color = tmp.bg_color;
      s_ch.attr.bit.reverse  = false;
      s_ch.attr.bit.standout = false;
      cc = &s_ch;
    }
    else if ( tmp.attr.bit.inherit_background )
    {
      // Add the covered background to this character
      std::memcpy (&i_ch, &tmp, sizeof(i_ch));
      i_ch.bg_color = cc->bg_color;  // last background color
      cc = &i_ch;
    }
    else  // default
      cc = &tmp;
  }
}

//----------------------------------------------------------------------
bool FVTerm::clearFullArea (const FTermArea* area, FChar& nc) const
{
  // Clear area
  const int area_size = area->width * area->height;
  std::fill_n (area->data, area_size, nc);

  if ( area != vdesktop )  // Is the area identical to the desktop?
    return false;

  // Try to clear the terminal rapidly with a control sequence
  if ( foutput->clearTerm (nc.ch[0]) )
  {
    nc.attr.bit.printed = true;
    std::fill_n (vterm->data, area_size, nc);
  }
  else
  {
    for (auto i{0}; i < vdesktop->height; i++)
    {
      vdesktop->changes[i].xmin = 0;
      vdesktop->changes[i].xmax = uInt(vdesktop->width) - 1;
      vdesktop->changes[i].trans_count = 0;
    }

    vdesktop->has_changes = true;
  }

  return true;
}

//----------------------------------------------------------------------
void FVTerm::clearAreaWithShadow (const FTermArea* area, const FChar& nc)
{
  FChar t_char = nc;
  const int total_width = getFullAreaWidth(area);
  t_char.attr.bit.transparent = true;

  for (auto y{0}; y < area->height; y++)
  {
    const int pos = y * total_width;
    // Clear area
    std::fill_n (&area->data[pos], total_width, nc);
    // Make right shadow transparent
    std::fill_n (&area->data[pos + area->width], area->right_shadow, t_char);
  }

  // Make bottom shadow transparent
  for (auto y{0}; y < area->bottom_shadow; y++)
  {
    const int pos = total_width * (y + area->height);
    std::fill_n (&area->data[pos], total_width, t_char);
  }
}

//----------------------------------------------------------------------
bool FVTerm::printWrap (FTermArea* area) const
{
  bool end_of_area{false};
  const int width  = area->width;
  const int height = area->height;
  const int rsh    = area->right_shadow;
  const int bsh    = area->bottom_shadow;

  // Line break at right margin
  if ( area->cursor_x > width + rsh )
  {
    area->cursor_x = 1;
    area->cursor_y++;
  }

  // Prevent up scrolling
  if ( area->cursor_y > height + bsh )
  {
    area->cursor_y--;
    end_of_area = true;
  }

  return end_of_area;
}

//----------------------------------------------------------------------
inline bool FVTerm::changedToTransparency (const FChar& from, const FChar& to) const
{
  return ( ( ! from.attr.bit.transparent && to.attr.bit.transparent )
        || ( ! from.attr.bit.color_overlay && to.attr.bit.color_overlay )
        || ( ! from.attr.bit.inherit_background && to.attr.bit.inherit_background ) )
    ? true
    : false;
}

//----------------------------------------------------------------------
inline bool FVTerm::changedFromTransparency (const FChar& from, const FChar& to) const
{
  return changedToTransparency(to, from) ? true : false;
}

//----------------------------------------------------------------------
inline void FVTerm::printCharacterOnCoordinate ( FTermArea* area
                                               , const int& ax
                                               , const int& ay
                                               , const FChar& ch) const
{
  if ( area->cursor_x <= 0
    || area->cursor_y <= 0
    || ax >= getFullAreaWidth(area)
    || ay >= getFullAreaHeight(area) )
    return;

  const int line_len = getFullAreaWidth(area);
  auto& ac = area->data[ay * line_len + ax];  // area character

  if ( ac != ch )  // compare with an overloaded operator
  {
    if ( changedToTransparency(ac, ch) )
    {
      // add one transparent character form line
      area->changes[ay].trans_count++;
    }

    if ( changedFromTransparency(ac, ch) )
    {
      // remove one transparent character from line
      area->changes[ay].trans_count--;
    }

    // copy character to area
    std::memcpy (&ac, &ch, sizeof(ac));

    if ( ax < int(area->changes[ay].xmin) )
      area->changes[ay].xmin = uInt(ax);

    if ( ax > int(area->changes[ay].xmax) )
      area->changes[ay].xmax = uInt(ax);
  }
}

//----------------------------------------------------------------------
void FVTerm::printPaddingCharacter (FTermArea* area, const FChar& term_char)
{
  // Creates a padding-character from the current character (term_char)
  // and prints it. It is a placeholder for the column after
  // a full-width character.

  FChar pc;  // padding character

  // Copy character to padding character
  std::memcpy (&pc, &term_char, sizeof(pc));

  if ( foutput->getEncoding() == Encoding::UTF8 )
  {
    pc.ch = {{ L'\0' }};
    pc.attr.bit.fullwidth_padding = true;
    pc.attr.bit.char_width = 0;
  }
  else
  {
    pc.ch[0] = L'.';
    pc.ch[1] = L'\0';
    pc.attr.bit.char_width = 1;
  }

  // Print the padding-character
  print (area, pc);
}

//----------------------------------------------------------------------
bool FVTerm::isInsideTerminal (const FPoint& pos) const
{
  // Check whether the coordinates are within the virtual terminal

  const FRect term_geometry {0, 0, foutput->getColumnNumber(), foutput->getLineNumber()};

  if ( term_geometry.contains(pos) )
    return true;
  else
    return false;
}

//----------------------------------------------------------------------
bool FVTerm::hasPendingUpdates (const FTermArea* area)
{
  return ( area && area->has_changes ) ? true : false;
}

}  // namespace finalcut
