/***********************************************************************
* fstatusbar.cpp - Widget FStatusBar and FStatusKey                    *
*                                                                      *
* This file is part of the Final Cut widget toolkit                    *
*                                                                      *
* Copyright 2014-2019 Markus Gans                                      *
*                                                                      *
* The Final Cut is free software; you can redistribute it and/or       *
* modify it under the terms of the GNU Lesser General Public License   *
* as published by the Free Software Foundation; either version 3 of    *
* the License, or (at your option) any later version.                  *
*                                                                      *
* The Final Cut is distributed in the hope that it will be useful,     *
* but WITHOUT ANY WARRANTY; without even the implied warranty of       *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
* GNU Lesser General Public License for more details.                  *
*                                                                      *
* You should have received a copy of the GNU Lesser General Public     *
* License along with this program.  If not, see                        *
* <http://www.gnu.org/licenses/>.                                      *
***********************************************************************/

#include <vector>

#include "final/fevent.h"
#include "final/fstatusbar.h"
#include "final/fwidgetcolors.h"

namespace finalcut
{

//----------------------------------------------------------------------
// class FStatusKey
//----------------------------------------------------------------------

// constructor and destructor
//----------------------------------------------------------------------
FStatusKey::FStatusKey(FWidget* parent)
  : FWidget(parent)
{
  init (parent);
}

//----------------------------------------------------------------------
FStatusKey::FStatusKey (FKey k, const FString& txt, FWidget* parent)
  : FWidget(parent)
  , text(txt)
  , key(k)
{
  init (parent);
}

//----------------------------------------------------------------------
FStatusKey::~FStatusKey()  // destructor
{
  if ( getConnectedStatusbar() )
    getConnectedStatusbar()->remove(this);

  delAccelerator();
}


// public methods of FStatusKey
//----------------------------------------------------------------------
void FStatusKey::onAccel (FAccelEvent* ev)
{
  if ( isActivated() )
    return;

  setActive();

  if ( getConnectedStatusbar() )
    getConnectedStatusbar()->redraw();

  ev->accept();
  // unset after get back from callback
  unsetActive();

  if ( getConnectedStatusbar() )
    getConnectedStatusbar()->redraw();
}

//----------------------------------------------------------------------
void FStatusKey::setActive()
{
  active = true;
  processActivate();
}

//----------------------------------------------------------------------
bool FStatusKey::setMouseFocus(bool enable)
{
  if ( mouse_focus == enable )
    return true;

  return (mouse_focus = enable);
}


// private methods of FStatusKey
//----------------------------------------------------------------------
void FStatusKey::init (FWidget* parent)
{
  setGeometry (FPoint(1, 1), FSize(1, 1));

  if ( parent && parent->isInstanceOf("FStatusBar") )
  {
    setConnectedStatusbar (static_cast<FStatusBar*>(parent));

    if ( getConnectedStatusbar() )
      getConnectedStatusbar()->insert(this);
  }
}

//----------------------------------------------------------------------
void FStatusKey::processActivate()
{
  emitCallback("activate");
}


//----------------------------------------------------------------------
// class FStatusBar
//----------------------------------------------------------------------

// constructor and destructor
//----------------------------------------------------------------------
FStatusBar::FStatusBar(FWidget* parent)
  : FWindow(parent)
{
  init();
}

//----------------------------------------------------------------------
FStatusBar::~FStatusBar()  // destructor
{
  // delete all keys
  if ( ! key_list.empty() )
  {
    auto iter = key_list.begin();

    while ( iter != key_list.end() )
    {
      (*iter)->setConnectedStatusbar(nullptr);
      delAccelerator (*iter);
      iter = key_list.erase(iter);
    }
  }

  setStatusBar(nullptr);
}


// public methods of FStatusBar
//----------------------------------------------------------------------
void FStatusBar::setMessage (const FString& mgs)
{
  text.setString(mgs);
}

//----------------------------------------------------------------------
bool FStatusBar::hasActivatedKey()
{
  if ( ! key_list.empty() )
  {
    for (auto&& k : key_list)
      if ( k->isActivated() )
        return true;
  }

  return false;
}

//----------------------------------------------------------------------
void FStatusBar::hide()
{
  const auto& wc = getFWidgetColors();
  FColor fg = wc.term_fg;
  FColor bg = wc.term_bg;
  setColor (fg, bg);
  print() << FPoint(1, 1) << FString(getDesktopWidth(), L' ');
  updateTerminal();
  FWindow::hide();
}

//----------------------------------------------------------------------
void FStatusBar::drawMessage()
{
  if ( ! (isVisible() ) )
    return;

  if ( x < 0 || x_msg < 0 )
    return;

  x = x_msg;
  int space_offset{1};
  bool hasKeys( ! key_list.empty() );
  bool isLastActiveFocus{false};
  std::size_t termWidth = getDesktopWidth();

  if ( hasKeys )
  {
    auto iter = key_list.end();
    isLastActiveFocus = bool ( (*(iter - 1))->isActivated()
                            || (*(iter - 1))->hasMouseFocus() );
  }
  else
    isLastActiveFocus = false;

  if ( isLastActiveFocus )
    space_offset = 0;

  const auto& wc = getFWidgetColors();
  setColor (wc.statusbar_fg, wc.statusbar_bg);
  setPrintPos (FPoint(x, 1));

  if ( isMonochron() )
    setReverse(true);

  if ( x + space_offset + 3 < int(termWidth) )
  {
    if ( text )
    {
      if ( ! isLastActiveFocus )
      {
        x++;
        print (' ');
      }

      if ( hasKeys )
      {
        x += 2;
        print (fc::BoxDrawingsVertical);  // │
        print (' ');
      }

      auto msg_length = getColumnWidth(getMessage());
      x += int(msg_length);

      if ( x - 1 <= int(termWidth) )
        print (getMessage());
      else
      {
        // Print ellipsis
        std::size_t len = msg_length + termWidth - uInt(x) - 1;
        print() << getColumnSubString ( getMessage(), 1, len)
                << "..";
      }
    }
  }

  for (int i = x; i <= int(termWidth); i++)
    print (' ');

  if ( isMonochron() )
    setReverse(false);
}

//----------------------------------------------------------------------
void FStatusBar::insert (FStatusKey* skey)
{
  key_list.push_back (skey);

  addAccelerator (skey->getKey(), skey);

  skey->addCallback
  (
    "activate",
    F_METHOD_CALLBACK (this, &FStatusBar::cb_statuskey_activated)
  );
}

//----------------------------------------------------------------------
void FStatusBar::remove (FStatusKey* skey)
{
  delAccelerator (skey);

  if ( key_list.empty() )
    return;

  auto iter = key_list.begin();

  while ( iter != key_list.end() )
  {
    if ( (*iter) == skey )
    {
      iter = key_list.erase(iter);
      skey->setConnectedStatusbar(nullptr);
      break;
    }
    else
      ++iter;
  }
}

//----------------------------------------------------------------------
void FStatusBar::remove (int pos)
{
  if ( int(getCount()) < pos )
    return;

  key_list.erase (key_list.begin() + pos - 1);
}

//----------------------------------------------------------------------
void FStatusBar::clear()
{
  key_list.clear();
  key_list.shrink_to_fit();
}

//----------------------------------------------------------------------
void FStatusBar::adjustSize()
{
  setGeometry ( FPoint(1, int(getDesktopHeight()))
              , FSize(getDesktopWidth(), 1), false );
}

//----------------------------------------------------------------------

void FStatusBar::onMouseDown (FMouseEvent* ev)
{
  if ( hasActivatedKey() )
    return;

  if ( ev->getButton() != fc::LeftButton )
  {
    mouse_down = false;

    if ( ! key_list.empty() )
    {
      auto iter = key_list.begin();
      auto last = key_list.end();

      while ( iter != last )
      {
        (*iter)->unsetMouseFocus();
        ++iter;
      }
    }

    redraw();
    return;
  }

  if ( mouse_down )
    return;

  mouse_down = true;

  if ( ! key_list.empty() )
  {
    int X{1};
    auto iter = key_list.begin();
    auto last = key_list.end();

    while ( iter != last )
    {
      int x1 = X
        , kname_len = getKeyNameWidth(*iter)
        , txt_length = getKeyTextWidth(*iter)
        , x2 = x1 + kname_len + txt_length + 1
        , mouse_x = ev->getX()
        , mouse_y = ev->getY();

      if ( mouse_x >= x1
        && mouse_x <= x2
        && mouse_y == 1
        && ! (*iter)->hasMouseFocus() )
      {
        (*iter)->setMouseFocus();
        redraw();
      }

      X = x2 + 2;
      ++iter;
    }
  }
}

//----------------------------------------------------------------------
void FStatusBar::onMouseUp (FMouseEvent* ev)
{
  if ( hasActivatedKey() )
    return;

  if ( ev->getButton() != fc::LeftButton )
    return;

  if ( mouse_down )
  {
    mouse_down = false;

    if ( ! key_list.empty() )
    {
      int X{1};
      auto iter = key_list.begin();
      auto last = key_list.end();

      while ( iter != last )
      {
        int x1 = X
          , kname_len = getKeyNameWidth(*iter)
          , txt_length = getKeyTextWidth(*iter)
          , x2 = x1 + kname_len + txt_length + 1;

        if ( (*iter)->hasMouseFocus() )
        {
          (*iter)->unsetMouseFocus();
          int mouse_x = ev->getX();
          int mouse_y = ev->getY();

          if ( mouse_x >= x1 && mouse_x <= x2 && mouse_y == 1 )
            (*iter)->setActive();

          // unset after get back from callback
          (*iter)->unsetActive();
          redraw();
        }

        X = x2 + 2;
        ++iter;
      }
    }
  }
}

//----------------------------------------------------------------------
void FStatusBar::onMouseMove (FMouseEvent* ev)
{
  if ( hasActivatedKey() )
    return;

  if ( ev->getButton() != fc::LeftButton )
    return;

  if ( mouse_down && ! key_list.empty() )
  {
    bool focus_changed{false};
    int X{1};
    auto iter = key_list.begin();
    auto last = key_list.end();

    while ( iter != last )
    {
      int x1 = X
        , kname_len = getKeyNameWidth(*iter)
        , txt_length = getKeyTextWidth(*iter)
        , x2 = x1 + kname_len + txt_length + 1
        , mouse_x = ev->getX()
        , mouse_y = ev->getY();

      if ( mouse_x >= x1
        && mouse_x <= x2
        && mouse_y == 1 )
      {
        if ( ! (*iter)->hasMouseFocus() )
        {
          (*iter)->setMouseFocus();
          focus_changed = true;
        }
      }
      else
      {
        if ( (*iter)->hasMouseFocus() )
        {
          (*iter)->unsetMouseFocus();
          focus_changed = true;
        }
      }

      X = x2 + 2;
      ++iter;
    }

    if ( focus_changed )
      redraw();
  }
}

//----------------------------------------------------------------------
void FStatusBar::cb_statuskey_activated (FWidget* widget, FDataPtr)
{
  if ( ! key_list.empty() )
  {
    auto statuskey = static_cast<FStatusKey*>(widget);
    auto iter = key_list.begin();
    auto last = key_list.end();

    while ( iter != last )
    {
      if ( (*iter) != statuskey && (*iter)->isActivated() )
        (*iter)->unsetActive();
      ++iter;
    }
  }

  redraw();
}


// private methods of FStatusBar
//----------------------------------------------------------------------
void FStatusBar::init()
{
  auto r = getRootWidget();
  std::size_t w = r->getWidth();
  int h = int(r->getHeight());
  // initialize geometry values
  setGeometry (FPoint(1, h), FSize(w, 1), false);
  setAlwaysOnTop();
  setStatusBar(this);
  ignorePadding();
  mouse_down = false;

  if ( getRootWidget() )
    getRootWidget()->setBottomPadding(1, true);

  const auto& wc = getFWidgetColors();
  setForegroundColor (wc.statusbar_fg);
  setBackgroundColor (wc.statusbar_bg);
  unsetFocusable();
}

//----------------------------------------------------------------------
int FStatusBar::getKeyNameWidth (const FStatusKey* key)
{
  const FString& key_name = getKeyName(key->getKey());
  return int(getColumnWidth(key_name));
}

//----------------------------------------------------------------------
int FStatusBar::getKeyTextWidth (const FStatusKey* key)
{
  const FString& key_text = key->getText();
  return int(getColumnWidth(key_text));
}

//----------------------------------------------------------------------
void FStatusBar::draw()
{
  drawKeys();
  drawMessage();
}

//----------------------------------------------------------------------
void FStatusBar::drawKeys()
{
  screenWidth = getDesktopWidth();
  x = 1;

  if ( key_list.empty() )
  {
    x_msg = 1;
    return;
  }

  print() << FPoint(1, 1);

  if ( isMonochron() )
    setReverse(true);

  auto iter = key_list.begin();

  while ( iter != key_list.end() )
  {
    auto item = *iter;
    keyname_len = getKeyNameWidth(item);

    if ( x + keyname_len + 2 < int(screenWidth) )
    {
      if ( item->isActivated() || item->hasMouseFocus() )
        drawActiveKey (iter);
      else
        drawKey (iter);
    }
    else
    {
      const auto& wc = getFWidgetColors();
      setColor (wc.statusbar_fg, wc.statusbar_bg);

      for (; x <= int(screenWidth); x++)
        print (' ');
    }

    ++iter;
  }

  if ( isMonochron() )
    setReverse(false);

  x_msg = x;
}

//----------------------------------------------------------------------
void FStatusBar::drawKey (keyList::const_iterator iter)
{
  // Draw not active key

  auto item = *iter;
  const auto& wc = getFWidgetColors();
  setColor (wc.statusbar_hotkey_fg, wc.statusbar_hotkey_bg);
  x++;
  print (' ');
  x += keyname_len;
  print (getKeyName(item->getKey()));
  setColor (wc.statusbar_fg, wc.statusbar_bg);
  x++;
  print ('-');
  auto column_width = getColumnWidth (item->getText());
  x += int(column_width);

  if ( x - 1 <= int(screenWidth) )
    print (item->getText());
  else
  {
    // Print ellipsis
    std::size_t len = column_width + screenWidth - std::size_t(x) - 1;
    print() << getColumnSubString(item->getText(), 1, len)
            << "..";
  }

  if ( iter + 1 != key_list.end()
    && ( (*(iter + 1))->isActivated() || (*(iter + 1))->hasMouseFocus() )
    && x + getKeyNameWidth(*(iter + 1)) + 3 < int(screenWidth) )
  {
    // Next element is active
    if ( isMonochron() )
      setReverse(false);

    if ( hasHalfBlockCharacter() )
    {
      setColor (wc.statusbar_active_fg, wc.statusbar_active_bg);
      print (fc::LeftHalfBlock);  // ▐
    }
    else
      print (' ');

    x++;

    if ( isMonochron() )
      setReverse(true);
  }
  else if ( iter + 1 != key_list.end() && x < int(screenWidth) )
  {
    // Not the last element
    setColor (wc.statusbar_separator_fg, wc.statusbar_bg);
    x++;
    print (fc::BoxDrawingsVertical);  // │
  }
}

//----------------------------------------------------------------------
void FStatusBar::drawActiveKey (keyList::const_iterator iter)
{
  // Draw active key

  auto item = *iter;

  if ( isMonochron() )
    setReverse(false);

  const auto& wc = getFWidgetColors();
  setColor ( wc.statusbar_active_hotkey_fg
           , wc.statusbar_active_hotkey_bg );
  x++;
  print (' ');
  x += keyname_len;
  print (getKeyName(item->getKey()));
  setColor (wc.statusbar_active_fg, wc.statusbar_active_bg);
  x++;
  print ('-');
  auto column_width = getColumnWidth (item->getText());
  x += int(column_width);

  if ( x <= int(screenWidth) )
  {
    print (item->getText());
    x++;
    print (fc::RightHalfBlock);  // ▌
  }
  else
  {
    // Print ellipsis
    std::size_t len = column_width + screenWidth - std::size_t(x) - 1;
    print() << getColumnSubString(item->getText(), 1, len)
            << "..";
  }

  if ( isMonochron() )
    setReverse(true);
}

}  // namespace finalcut
