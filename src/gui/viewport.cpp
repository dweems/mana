/*
 *  The Mana World
 *  Copyright 2004 The Mana World Development Team
 *
 *  This file is part of The Mana World.
 *
 *  The Mana World is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  any later version.
 *
 *  The Mana World is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with The Mana World; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  $Id$
 */

#include "viewport.h"

#include <guichan/sdl/sdlinput.hpp>

#include "gui.h"
#include "popupmenu.h"

#include "../beingmanager.h"
#include "../configuration.h"
#include "../flooritemmanager.h"
#include "../graphics.h"
#include "../localplayer.h"
#include "../map.h"
#include "../npc.h"

#include "../resources/monsterdb.h"

#include "../utils/tostring.h"

Viewport::Viewport():
    mMap(0),
    mViewX(0.0f),
    mViewY(0.0f),
    mCameraX(0),
    mCameraY(0),
    mShowDebugPath(false),
    mPopupActive(false)
{
    setOpaque(false);
    addMouseListener(this);

    mScrollLaziness = (int) config.getValue("ScrollLaziness", 32);
    mScrollRadius = (int) config.getValue("ScrollRadius", 32);

    config.addListener("ScrollLaziness", this);
    config.addListener("ScrollRadius", this);

    mPopupMenu = new PopupMenu();
}

Viewport::~Viewport()
{
    delete mPopupMenu;
}

void
Viewport::setMap(Map *map)
{
    mMap = map;
}

void
Viewport::draw(gcn::Graphics *gcnGraphics)
{
    static int lastTick = tick_time;

    if (!mMap || !player_node)
        return;

    Graphics *graphics = static_cast<Graphics*>(gcnGraphics);

    // Avoid freaking out when tick_time overflows
    if (tick_time < lastTick)
    {
        lastTick = tick_time;
    }

    // Calculate viewpoint
    int midTileX = graphics->getWidth() / 32 / 2;
    int midTileY = graphics->getHeight() / 32 / 2;

    int player_x = (player_node->mX - midTileX) * 32 +
                    player_node->getXOffset();
    int player_y = (player_node->mY - midTileY) * 32 +
                    player_node->getYOffset();

    if (mScrollLaziness < 1)
        mScrollLaziness = 1; // Avoids division by zero

    // Apply lazy scrolling
    while (lastTick < tick_time)
    {
        if (player_x > mViewX + mScrollRadius)
        {
            mViewX += (player_x - mViewX - mScrollRadius) / mScrollLaziness;
        }
        if (player_x < mViewX - mScrollRadius)
        {
            mViewX += (player_x - mViewX + mScrollRadius) / mScrollLaziness;
        }
        if (player_y > mViewY + mScrollRadius)
        {
            mViewY += (player_y - mViewY - mScrollRadius) / mScrollLaziness;
        }
        if (player_y < mViewY - mScrollRadius)
        {
            mViewY += (player_y - mViewY + mScrollRadius) / mScrollLaziness;
        }
        lastTick++;
    }

    // Auto center when player is off screen
    if (        player_x - mViewX > graphics->getWidth() / 2
            ||  mViewX - player_x > graphics->getWidth() / 2
            ||  mViewY - player_y > graphics->getHeight() / 2
            ||  player_y - mViewY > graphics->getHeight() / 2
        )
    {
        mViewX = player_x;
        mViewY = player_y;
    };

    if (mMap) {
        if (mViewX < 0) {
            mViewX = 0;
        }
        if (mViewY < 0) {
            mViewY = 0;
        }
        if (mViewX > (mMap->getWidth() - midTileX) * 32) {
            mViewX = (mMap->getWidth() - midTileX) * 32;
        }
        if (mViewY > (mMap->getHeight() - midTileY) * 32) {
            mViewY = (mMap->getHeight() - midTileY) * 32;
        }
    }

    mCameraX = (int) (mViewX + 16) / 32;
    mCameraY = (int) (mViewY + 16) / 32;

    // Draw tiles and sprites
    if (mMap)
    {
        mMap->draw(graphics, (int) mViewX, (int) mViewY, 0);
        mMap->draw(graphics, (int) mViewX, (int) mViewY, 1);
        mMap->draw(graphics, (int) mViewX, (int) mViewY, 2);
        mMap->drawOverlay(graphics, mViewX, mViewY,
                (int) config.getValue("OverlayDetail", 2));
    }

    // Find a path from the player to the mouse, and draw it. This is for debug
    // purposes.
    if (mShowDebugPath && mMap)
    {
        // Get the current mouse position
        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);

        int mouseTileX = mouseX / 32 + mCameraX;
        int mouseTileY = mouseY / 32 + mCameraY;

        Path debugPath = mMap->findPath(
                player_node->mX, player_node->mY,
                mouseTileX, mouseTileY);

        graphics->setColor(gcn::Color(255, 0, 0));
        for (PathIterator i = debugPath.begin(); i != debugPath.end(); i++)
        {
            int squareX = i->x * 32 - (int) mViewX + 12;
            int squareY = i->y * 32 - (int) mViewY + 12;

            graphics->fillRectangle(gcn::Rectangle(squareX, squareY, 8, 8));
            graphics->drawText(
                    toString(mMap->getMetaTile(i->x, i->y)->Gcost),
                    squareX + 4, squareY + 12, gcn::Graphics::CENTER);
        }
    }

    // Draw player nickname, speech, and emotion sprite as needed
    Beings &beings = beingManager->getAll();
    for (BeingIterator i = beings.begin(); i != beings.end(); i++)
    {
        (*i)->drawSpeech(graphics, -(int) mViewX, -(int) mViewY);
        (*i)->drawName(graphics, -(int) mViewX, -(int) mViewY);
        (*i)->drawEmotion(graphics, -(int) mViewX, -(int) mViewY);
    }

    // Draw target marker if needed
    Being *target = player_node->getTarget();
    if (target)
    {
        graphics->setFont(speechFont);
        graphics->setColor(gcn::Color(255, 32, 32));
        int dy = (target->getType() == Being::PLAYER) ? 80 : 42;

        std::string mobName = "";

        if (target->mJob >= 1002)
        {
            int mobId = target->mJob - 1002;
            mobName = MonsterDB::get(mobId).getName();

            graphics->drawText(mobName,
                    target->getPixelX() - (int) mViewX + 15,
                    target->getPixelY() - (int) mViewY - dy,
                    gcn::Graphics::CENTER);
        }
    }

    // Draw contained widgets
    WindowContainer::draw(gcnGraphics);
}

void
Viewport::logic()
{
    WindowContainer::logic();

    if (!mMap || !player_node)
        return;

    int mouseX, mouseY;
    Uint8 button = SDL_GetMouseState(&mouseX, &mouseY);

    if (mPlayerFollowMouse && button & SDL_BUTTON(1) &&
            mWalkTime != player_node->mWalkTime)
    {
        player_node->setDestination(mouseX / 32 + mCameraX,
                                    mouseY / 32 + mCameraY);
        mWalkTime = player_node->mWalkTime;
    }
}

void
Viewport::mousePress(int mx, int my, int button)
{
    // Check if we are alive and kickin'
    if (!mMap || !player_node || player_node->mAction == Being::DEAD)
        return;

    // Check if we are busy
    if (current_npc)
        return;

    mPlayerFollowMouse = false;

    int tilex = mx / 32 + mCameraX;
    int tiley = my / 32 + mCameraY;

    // Right click might open a popup
    if (button == gcn::MouseInput::RIGHT)
    {
        Being *being;
        FloorItem *floorItem;

        if ((being = beingManager->findBeing(tilex, tiley)) &&
                being->getType() != Being::LOCALPLAYER)
        {
            showPopup(mx, my, being);
            return;
        }
        else if((floorItem = floorItemManager->findByCoordinates(tilex, tiley)))
        {
            showPopup(mx, my, floorItem);
            return;
        }
    }

    // If a popup is active, just remove it
    if (mPopupActive)
    {
        mPopupMenu->setVisible(false);
        mPopupActive = false;
        return;
    }

    // Left click can cause different actions
    if (button == gcn::MouseInput::LEFT)
    {
        Being *being;
        FloorItem *item;

        // Interact with some being
        if ((being = beingManager->findBeing(tilex, tiley)))
        {
            switch (being->getType())
            {
                case Being::NPC:
                    dynamic_cast<NPC*>(being)->talk();
                    break;

                case Being::MONSTER:
                case Being::PLAYER:
                    if (being->mAction == Being::DEAD)
                        break;

                    player_node->attack(being, true);
                    break;

                default:
                    break;
            }
        }
        // Pick up some item
        else if ((item = floorItemManager->findByCoordinates(tilex, tiley)))
        {
                player_node->pickUp(item);
        }
        // Just walk around
        else if (mMap->getWalk(tilex, tiley))
        {
            // XXX XXX XXX REALLY UGLY!
            Uint8 *keys = SDL_GetKeyState(NULL);
            if (!(keys[SDLK_LSHIFT] || keys[SDLK_RSHIFT]))
            {
                player_node->setDestination(tilex, tiley);
                player_node->stopAttack();
            }
            mPlayerFollowMouse = true;
        }
    }

    if (button == gcn::MouseInput::MIDDLE)
    {
        // Find the being nearest to the clicked position
        Being *target = beingManager->findNearestLivingBeing(
                tilex, tiley,
                20, Being::MONSTER);

        if (target)
        {
            player_node->setTarget(target);
        }
    }
}

void
Viewport::mouseMotion(int mx, int my)
{
    if (!mMap || !player_node)
        return;

    if (mPlayerFollowMouse && mWalkTime == player_node->mWalkTime)
    {
        player_node->setDestination(mx / 32 + mCameraX, my / 32 + mCameraY);
    }
}

void
Viewport::mouseRelease(int mx, int my, int button)
{
    mPlayerFollowMouse = false;
}

void
Viewport::showPopup(int x, int y, Item *item)
{
    mPopupMenu->showPopup(x, y, item);
    mPopupActive = true;
}

void
Viewport::showPopup(int x, int y, FloorItem *floorItem)
{
    mPopupMenu->showPopup(x, y, floorItem);
    mPopupActive = true;
}

void
Viewport::showPopup(int x, int y, Being *being)
{
    mPopupMenu->showPopup(x, y, being);
    mPopupActive = true;
}

void
Viewport::optionChanged(const std::string &name)
{
    mScrollLaziness = (int) config.getValue("ScrollLaziness", 32);
    mScrollRadius = (int) config.getValue("ScrollRadius", 32);
}