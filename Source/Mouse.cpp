/*
 *  Open Fodder
 *  ---------------
 *
 *  Copyright (C) 2008-2026 Open Fodder
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "stdafx.hpp"

const int16 MOUSE_POSITION_X_ADJUST = -32;
const int16 MOUSE_POSITION_Y_ADJUST = 4;

void cFodder::Mouse_Setup() {

    mMouse_EventLastButtonsPressed = 0;
    mButtonPressLeft = 0;
    mButtonPressRight = 0;
    mMouseButtonStatus = 0;

    mMouseX = (getCameraWidth() / 2) - 9;
    mMouseY = (getCameraHeight() / 2) - 9;
    mInputMouseX = mMouseX;
    mInputMouseY = mMouseY;
}

void cFodder::Mouse_Cursor_Handle() {
	static int32 CaptureBlockFrames = 0;
    static bool MouseCapturedGlobal = false;
	const cDimension scale = mWindow->GetScale();
    static bool CursorCaptured = true;
    const int edgePadding = 2;
    const uint64 recaptureDelayMs = 150;

	mMouseButtonStatus = mMouse_EventLastButtonsPressed;

    auto UpdateInputFromWindowPos = [&]() {
        float x = 0.0f;
        float y = 0.0f;
        SDL_GetMouseState(&x, &y);
        mInputMouseX = (static_cast<int>(x) / scale.getWidth()) + MOUSE_POSITION_X_ADJUST;
        mInputMouseY = (static_cast<int>(y) / scale.getHeight()) + MOUSE_POSITION_Y_ADJUST;
    };

    auto ReleaseCursor = [&](bool pWarpToEdge) {
        CursorCaptured = false;
        mMouse_LeftWindow = static_cast<uint64>(SDL_GetTicks());
        mWindow->SetRelativeMouseMode(false);
        SDL_GetRelativeMouseState(nullptr, nullptr);
        mMouse_EventLastPositionRelative = { 0, 0 };
        SDL_ShowCursor();
        mWindow_MouseInside = false;

        if (MouseCapturedGlobal) {
            SDL_CaptureMouse(false);
            MouseCapturedGlobal = false;
        }

        if (!pWarpToEdge)
            return;

        const cDimension windowSize = mWindow->GetWindowSize();
        int x = (mMouseX - MOUSE_POSITION_X_ADJUST) * scale.getWidth();
        int y = (mMouseY - MOUSE_POSITION_Y_ADJUST) * scale.getHeight();
        if (x < 0)
            x = 0;
        else if (x >= windowSize.getWidth())
            x = windowSize.getWidth() - 1;
        if (y < 0)
            y = 0;
        else if (y >= windowSize.getHeight())
            y = windowSize.getHeight() - 1;
        mWindow->SetMousePositionInWindow(static_cast<float>(x), static_cast<float>(y));
    };

	if (mStartParams->mMouseAlternative) {
		mInputMouseX = (mMouse_EventLastPosition.mX / scale.getWidth()) + MOUSE_POSITION_X_ADJUST;
		mInputMouseY = (mMouse_EventLastPosition.mY / scale.getHeight()) + MOUSE_POSITION_Y_ADJUST;
		return;
	}

    // Keyboard+mouse mode owns cursor capture (see Mouse_Inputs_Check_KeyboardMouse).
    // Skip the classic-mode focus/border release dance entirely and just
    // drain accumulated relative motion — otherwise on focus-loss the
    // classic path disables relative mode and then gates re-capture
    // behind an edge-padding guard, which strands mMouse_EventLastPositionRelative
    // undrained and freezes the in-game cursor.
    if (mKeyboardMouse_Mode && mPhase_In_Progress) {
        mInputMouseX = mMouseX + static_cast<int16>(mMouse_EventLastPositionRelative.mX);
        mInputMouseY = mMouseY + static_cast<int16>(mMouse_EventLastPositionRelative.mY);
        mMouse_EventLastPositionRelative = { 0, 0 };
        return;
    }

    if (!mWindow_Focus) {
        if (CursorCaptured) {
            CursorCaptured = false;
            mWindow->SetRelativeMouseMode(false);
            SDL_GetRelativeMouseState(nullptr, nullptr);
            mMouse_EventLastPositionRelative = { 0, 0 };
            SDL_ShowCursor();
        }
        mWindow_MouseInside = false;
        if (MouseCapturedGlobal) {
            SDL_CaptureMouse(false);
            MouseCapturedGlobal = false;
        }
        if (mWindow->isMouseInside()) {
            // Register mouse position even when not focused but cursor on window
            UpdateInputFromWindowPos();
        }
        return;
    }

    // Free cursor mode: wait for window mouse enter event to recapture.
    if (!CursorCaptured) {
        const uint64 now = static_cast<uint64>(SDL_GetTicks());
        const bool canRecapture = (now - mMouse_LeftWindow) > recaptureDelayMs;
        const bool inside = mWindow_MouseInside || mWindow->isMouseInside();

        if (canRecapture && inside) {
            float wx = 0.0f, wy = 0.0f;
            SDL_GetMouseState(&wx, &wy);
            const cDimension windowSize = mWindow->GetWindowSize();
            const bool awayFromEdge =
                (wx > edgePadding) &&
                (wy > edgePadding) &&
                (wx < static_cast<float>(windowSize.getWidth() - edgePadding)) &&
                (wy < static_cast<float>(windowSize.getHeight() - edgePadding));
            if (!awayFromEdge)
                return;

            UpdateInputFromWindowPos();
            mMouseX = mInputMouseX;
            mMouseY = mInputMouseY;

            CaptureBlockFrames = 1;
            CursorCaptured = true;
            mWindow->SetRelativeMouseMode(true);
            SDL_HideCursor();
            SDL_GetRelativeMouseState(nullptr, nullptr);
            mMouse_EventLastPositionRelative = { 0, 0 };
        }
        return;
    }

    if (!mParams->mMouseLocked && !mWindow->isFullscreen()) {
        if (Mouse_IsOnBorder() && !mMouseButtonStatus) {
            ReleaseCursor(true);
            return;
        }
    }

    // hack to avoid moving cursor on window resizing
    if (mWindow->isResized()) {
        mWindow->ClearResized();

    } else {
        if (CaptureBlockFrames > 0) {
            --CaptureBlockFrames;
        } else {
            // Calc the distance from the cursor to the centre of the window
            mInputMouseX = mMouseX + static_cast<int16>(mMouse_EventLastPositionRelative.mX);
            mInputMouseY = mMouseY + static_cast<int16>(mMouse_EventLastPositionRelative.mY);
            mMouse_EventLastPositionRelative = { 0,0 };
        }
    }
}

bool cFodder::Mouse_IsOnBorder() const {
    const cDimension ScreenSize = mWindow->GetScreenSize();

    if (mMouseX <= MOUSE_POSITION_X_ADJUST)
        return true;
    if (mMouseX >= ScreenSize.getWidth() + MOUSE_POSITION_X_ADJUST - 1)
        return true;
    if (mMouseY <= MOUSE_POSITION_Y_ADJUST)
        return true;
    if (mMouseY >= ScreenSize.getHeight() + MOUSE_POSITION_Y_ADJUST - 1)
        return true;

    return false;
}

void cFodder::Mouse_ReadInputs() {

    if (mParams->mDemoPlayback) {

        auto State = mGame_Data.mDemoRecorded.GetState(mGame_Data.mDemoRecorded.mTick);
        if (State) {
            mInputMouseX = State->mInputMouseX;
            mInputMouseY = State->mInputMouseY;

            mMouseButtonStatus = State->mMouseButtonStatus;
        }
        else {
            if(mGame_Data.mDemoRecorded.mTick > mGame_Data.mDemoRecorded.GetTotalTicks() + 100 )
                mPhase_Aborted = true;
        }
    }
    else {
        Mouse_Cursor_Handle();
    }

    if (mParams->mDemoRecord && !mParams->mDemoPlayback)
        mGame_Data.mDemoRecorded.AddState(mGame_Data.mDemoRecorded.mTick, cStateRecorded{ mInputMouseX, mInputMouseY, mMouseButtonStatus });

    Mouse_UpdateButtons();

    int16 Data4 = mInputMouseX;

    if (mSidebar_SmallMode == 0)
        goto loc_13B3A;

    if (Data4 >= MOUSE_POSITION_X_ADJUST + 16)
        goto loc_13B58;

    goto loc_13B41;

loc_13B3A:;
    if (Data4 >= MOUSE_POSITION_X_ADJUST)
        goto loc_13B58;

loc_13B41:;
    if (mSidebar_SmallMode)
        Data4 = MOUSE_POSITION_X_ADJUST + 16;
    else
        Data4 = MOUSE_POSITION_X_ADJUST;

    goto loc_13B66;

loc_13B58:;

    if (Data4 > (int16)mWindow->GetScreenSize().getWidth() + MOUSE_POSITION_X_ADJUST - 1)
        Data4 = (int16)mWindow->GetScreenSize().getWidth() + MOUSE_POSITION_X_ADJUST - 1;

loc_13B66:;
    mMouseX = Data4;

    int16 Data0 = mInputMouseY;

    if (Data0 < MOUSE_POSITION_Y_ADJUST)
        Data0 = MOUSE_POSITION_Y_ADJUST;
    else {

        if (Data0 > (int16)mWindow->GetScreenSize().getHeight() + MOUSE_POSITION_Y_ADJUST - 1)
            Data0 = (int16)mWindow->GetScreenSize().getHeight() + MOUSE_POSITION_Y_ADJUST - 1;
    }

    mMouseY = Data0;
}

void cFodder::Mouse_UpdateButtons() {

    mButtonPressLeft = 0;
    if (mMouseButtonStatus & 1) {
        mButtonPressLeft -= 1;
        if (mMouse_Button_Left_Toggle == 0) {
            mMouse_Button_Left_Toggle = -1;
            mMouse_Exit_Loop = true;

            if (mButtonPressRight) {
                mMouse_Button_LeftRight_Toggle = true;
                mMouse_Button_LeftRight_Toggle2 = true;
            }
        }

    }
    else {
        mMouse_Button_LeftRight_Toggle2 = false;
        mMouse_Button_Left_Toggle = 0;
    }

    mButtonPressRight = 0;
    if (mMouseButtonStatus & 2) {
        mButtonPressRight -= 1;
        if (mMouse_Button_Right_Toggle == 0) {
            mSquad_Member_Fire_CoolDown_Override = true;
            mMouse_Button_Right_Toggle = -1;
        }
    }
    else {
        mMouse_Button_Right_Toggle = 0;
    }

}

void cFodder::Mouse_DrawCursor() {
    if (mParams->mDisableVideo)
        return;

    mVideo_Draw_PosX = (mMouseX + mMouseX_Offset) + SIDEBAR_WIDTH;
    mVideo_Draw_PosY = (mMouseY + mMouseY_Offset) + 12;

    if (mMouseSpriteNew >= 0) {
        mMouseSpriteCurrent = mMouseSpriteNew;
        mMouseSpriteNew = -1;
    }

    if (mGraphics)
        mGraphics->Mouse_DrawCursor();
}

void cFodder::Mouse_UpdateCursor() {
    int16 Data0, Data4, Data8;

    // Sidebar
    if (mMouseX < 32) {
        mMouseSpriteNew = eSprite_pStuff_Mouse_Cursor;
        mMouseX_Offset = 0;
        mMouseY_Offset = 0;
        return;
    }

    if (!mButtonPressRight) {

        Data0 = mMouseX + (mCameraX >> 16);
        Data4 = mMouseY + (mCameraY >> 16);

        Data0 -= 0x0F;
        Data4 -= 3;

        //loc_30B36
        for (auto& Vehicle : mSprites_HumanVehicles) {

            if (Vehicle->mPosX < 0)
                continue;

            if (!Vehicle->mVehicleEnabled)
                continue;

            // If not human
            if (Vehicle->mPersonType != eSprite_PersonType_Human)
                continue;

            Data8 = Vehicle->mPosX;
            if (Vehicle->mVehicleType == eVehicle_Turret_Cannon)
                goto loc_30BB9;
            if (Vehicle->mVehicleType == eVehicle_Turret_Missile)
                goto loc_30BBE;

        loc_30BB9:;
            Data8 -= 8;
        loc_30BBE:;

            if (Data0 < Data8)
                continue;

            Data8 += mSprite_Width[Vehicle->mSpriteType];
            if (Data0 > Data8)
                continue;

            Data8 = Vehicle->mPosY;
            Data8 -= Vehicle->mHeight;
            Data8 -= mSprite_Height_Top[Vehicle->mSpriteType];
            Data8 -= 0x14;

            if (Data4 < Data8)
                continue;

            Data8 = Vehicle->mPosY;
            Data8 -= Vehicle->mHeight;
            if (Data4 > Data8)
                continue;

            mMouseSetToCursor = -1;

            // Is vehicle off ground
            if (Vehicle->mHeight) {

                // And is current
                if (Vehicle != mSquad_CurrentVehicle)
                    return;

                if (Vehicle->mSheetIndex == 0xA5)
                    return;

                // Show the helicopter land icon
                mMouseSpriteNew = eSprite_pStuff_Mouse_Helicopter;
                mMouseX_Offset = 0;
                mMouseY_Offset = 0;
                return;
            }

            // Is this the vehicle the current squad is in
            if (Vehicle == mSquad_CurrentVehicle)
                mMouseSpriteNew = eSprite_pStuff_Mouse_Arrow_DownRight;
            else
                mMouseSpriteNew = eSprite_pStuff_Mouse_Arrow_UpLeft;

            mMouseX_Offset = 0;
            mMouseY_Offset = 0;
            return;
        }
    }

    if (mMouseSetToCursor) {
        mMouseSetToCursor = 0;
        mMouseSpriteNew = eSprite_pStuff_Mouse_Cursor;
        mMouseX_Offset = 0;
        mMouseY_Offset = 0;
    }
}

void cFodder::Mouse_Inputs_Check_KeyboardMouse() {
    if (!(mPhase_In_Progress && mSquad_Selected >= 0))
        return;

    if (mSquad_Leader == INVALID_SPRITE_PTR || mSquad_Leader == 0)
        return;

    // Called both before the GUI loop (so WASD keeps driving the
    // squad when the cursor is over the sidebar) and after it (the
    // existing caller). Only run the full body once per interrupt
    // tick — the post-GUI call then just falls through to its
    // click-toggle-consume + early return below.
    if (mKBM_LastHandledTick == (uint32)mInterruptTick)
        return;
    mKBM_LastHandledTick = (uint32)mInterruptTick;

    // Force cursor capture so the OS arrow is hidden in this mode
    if (!mParams->mMouseLocked) {
        mParams->mMouseLocked = true;
        if (mStartParams->mMouseAlternative)
            mWindow->SetRelativeMouseMode(true);
    }
    SDL_HideCursor();

    // --- Vehicle mode: WASD drives, mouse aims, clicks fire/exit ---
    if (mSquad_CurrentVehicle) {
        Mouse_Inputs_Vehicle_KeyboardMouse();
        return;
    }

    // Weapon mutex: when the current explosive runs dry, switch to the
    // other one if it has ammo. Keeps the sidebar from greying both
    // icons and ensures right-click always has a selected explosive.
    {
        const int sel = mSquad_Selected;
        const bool hasG = mSquad_Grenades[sel] > 0;
        const bool hasR = mSquad_Rockets[sel] > 0;
        const auto cur = mSquad_CurrentWeapon[sel];
        if (cur == eWeapon_Grenade && !hasG && hasR)
            Squad_Select_Rockets();
        else if (cur == eWeapon_Rocket && !hasR && hasG)
            Squad_Select_Grenades();
        else if (cur != eWeapon_Grenade && cur != eWeapon_Rocket) {
            if (hasG) Squad_Select_Grenades();
            else if (hasR) Squad_Select_Rockets();
        }
    }

    // Preserve the vehicle-enter cursor set by Mouse_UpdateCursor —
    // the reticle would hide the affordance, and a left-click here
    // should route to the classic vehicle-entry path.
    const bool hoveringVehicle =
        (mMouseSpriteNew == eSprite_pStuff_Mouse_Arrow_UpLeft ||
         mMouseSpriteNew == eSprite_pStuff_Mouse_Helicopter);

    // Cursor parked in the sidebar: Mouse_UpdateCursor already set
    // the arrow cursor; leave it alone so the sidebar affordances
    // show normally.
    const bool cursorInGameArea = (mMouseX >= 32);

    if (!hoveringVehicle && cursorInGameArea) {
        mMouseSpriteNew = eSprite_pStuff_Mouse_Target;
        mMouseX_Offset = -8;
        mMouseY_Offset = -8;
    }

    // --- Aim weapons continuously at mouse position ---
    int16 TargetX = mMouseX + (mCameraX >> 16);
    int16 TargetY = mMouseY + (mCameraY >> 16);
    TargetX -= 0x10;
    if (!TargetX)
        TargetX = 1;
    TargetY -= 8;

    for (sSprite** Data20 = mSquads[mSquad_Selected]; ; ) {
        if (*Data20 == INVALID_SPRITE_PTR || *Data20 == 0)
            break;
        sSprite* Data24 = *Data20++;
        Data24->mWeaponTargetX = TargetX;
        Data24->mWeaponTargetY = TargetY;
    }

    // --- Firing ---
    // Clicks on the sidebar belong to the GUI loop (squad select /
    // split / weapon switch). Gating by cursor position here keeps
    // the pre-GUI call idempotent: it does movement + aim in all
    // cases, but only fires when the cursor is over the game area.
    bool rightJustPressed = cursorInGameArea && (mMouse_Button_Right_Toggle < 0);
    bool leftJustPressed  = cursorInGameArea && (mMouse_Button_Left_Toggle < 0);

    if (rightJustPressed && !hoveringVehicle) {
        // Fire the currently-selected explosive (the mutex above
        // guarantees one is selected if either has ammo).
        // Sprite_Handle_Troop_Weapon gates grenade/rocket launch behind
        // mMouse_Button_LeftRight_Toggle (normally the "right-held +
        // left-tapped" classic gesture). Set it here so right-click
        // alone throws the explosive; the projectile code clears it
        // after launch.
        mMouse_Button_LeftRight_Toggle = true;
        mSquad_Member_Fire_CoolDown_Override = true;

        // Rocket fire pause: in classic the "hold right + tap left"
        // gesture incidentally stops the squad because right-click
        // suppresses the click-to-walk path. KBM fires on a single
        // tap, so stage the same pause manually — suppress WASD for
        // long enough that the rocket animation completes against a
        // stationary squad. Grenades are quick throws and don't need
        // it. Only triggers when a rocket will actually launch (one
        // is selected and the squad has ammo).
        if (mSquad_CurrentWeapon[mSquad_Selected] == eWeapon_Rocket
            && mSquad_Rockets[mSquad_Selected] > 0) {
            mKBM_FirePauseFrames = 28;
        }
    } else if (leftJustPressed) {
        if (hoveringVehicle) {
            // Classic vehicle-entry path: hit-tests the vehicle under
            // the cursor and sets mVehicleWalkTarget on each squad
            // member, which triggers enter-on-reach.
            Squad_Assign_Target_From_Mouse();
        } else {
            // Plain fire. Merge is handled below via WASD proximity —
            // left-click is just the trigger in KBM mode. Gun fires
            // whenever mMouse_Button_LeftRight_Toggle is clear
            // (Sprite_Handle_Troop_Weapon gates explosives on that
            // toggle, not on the weapon selection). Don't reset
            // mSquad_CurrentWeapon here — doing so lets the
            // weapon-mutex above re-select grenade on the next frame
            // and steal the right-click from rockets.
            mSquad_Member_Fire_CoolDown_Override = true;
        }
    }

    // WASD-driven squad merge: the classic merge requires clicking on
    // another squad's troop; KBM has no click-to-merge. Instead, arm
    // the merge automatically when the selected squad walks within
    // approach range of another squad. To prevent an instant
    // re-merge right after splitting (where troops are interleaved),
    // the merge only becomes eligible once the squads have been far
    // enough apart (separation gate).
    if (mSquad_Leader) {
        const int32 sepD2 = 30 * 30;
        const int32 appD2 = 24 * 24;

        const int16 leaderX = mSquad_Leader->mPosX;
        const int16 leaderY = mSquad_Leader->mPosY - mSquad_Leader->mHeight;

        sSprite* nearest    = nullptr;
        int32    nearestD2  = appD2 + 1;
        int32    minOtherD2 = INT32_MAX;

        for (auto& Troop : mGame_Data.mSoldiers_Allocated) {
            if (Troop.mSprite == INVALID_SPRITE_PTR || Troop.mSprite == 0)
                continue;
            sSprite* s = Troop.mSprite;
            if (s->mSpriteType != eSprite_Player)
                continue;
            if (s->field_32 < 0 || s->field_32 == mSquad_Selected)
                continue;

            const int32 tdx = s->mPosX - leaderX;
            const int32 tdy = (s->mPosY - s->mHeight) - leaderY;
            const int32 d2  = tdx * tdx + tdy * tdy;

            if (d2 < minOtherD2)
                minOtherD2 = d2;
            if (d2 < nearestD2) { nearestD2 = d2; nearest = s; }
        }

        if (minOtherD2 > sepD2)
            mKBM_MergeEligible[mSquad_Selected] = true;

        if (nearest && mKBM_MergeEligible[mSquad_Selected]
            && mSquad_Join_TargetSquad[mSquad_Selected] < 0) {

            const int8 targetSquadId = (int8)nearest->field_32;
            if (mSquads_TroopCount[targetSquadId] + mSquads_TroopCount[mSquad_Selected] <= 8) {
                mSquad_Join_TargetSprite[mSquad_Selected] = nearest;
                mSquad_Join_TargetSquad[mSquad_Selected]  = targetSquadId;
                Squad_Walk_Target_Reset(targetSquadId);
                mKBM_MergeEligible[mSquad_Selected] = false;
            }
        }
    }

    if (mButtonPressLeft || mButtonPressRight) {
        mSprite_Player_CheckWeapon = -1;
        if (word_3A9B8 < 0) {
            word_3A9B8 = 0x30;
            if (mSquad_Leader)
                mSquad_Leader->mWeaponFireTimer = 0;
        }
        Squad_Member_CanFire();
    }

    // --- Keyboard movement ---
    //
    // Press/turn: synthesize a 45°-preserved map-edge "click" in the
    // input direction and dispatch via Squad_Walk_Target_Set.
    //
    // Release: the anti-stack bump can't separate a squad at rest
    // (mNext race — see Sprite_Handle_Player_Close_To_SquadMember),
    // so instead we record the leader's walked path each tick and on
    // release snap each follower to its arc-length point along that
    // trail. That gives the original game's snake shape without any
    // overshoot or reliance on downstream bump timing.
    int16 dx = 0;
    int16 dy = 0;
    if (mKey_A_Pressed) dx -= 1;
    if (mKey_D_Pressed) dx += 1;
    if (mKey_W_Pressed) dy -= 1;
    if (mKey_S_Pressed) dy += 1;

    // Rocket-fire pause: pretend WASD is released so the existing
    // "release" branch below collapses the walk queue and snaps the
    // snake formation. WASD held down through the pause resumes
    // automatically once the counter elapses (the resulting dx/dy
    // mismatch with mKBM_LastDx/Dy fires the press branch).
    if (mKBM_FirePauseFrames > 0) {
        --mKBM_FirePauseFrames;
        dx = 0;
        dy = 0;
    }

    const bool WasIdle = (mKBM_LastDx == 0 && mKBM_LastDy == 0);
    const bool NowIdle = (dx == 0 && dy == 0);

    // Fresh press: drop any stale trail from a prior movement session.
    if (WasIdle && !NowIdle) {
        mKBM_LeaderTrailHead = 0;
        mKBM_LeaderTrailCount = 0;
    }

    // Sample the leader's current position into the trail every frame
    // we're in a walking state. Skip duplicates so the trail always
    // has strictly monotonic samples for arc-length math.
    if (!WasIdle || !NowIdle) {
        const int16 lx = mSquad_Leader->mPosX;
        const int16 ly = mSquad_Leader->mPosY;
        bool push = (mKBM_LeaderTrailCount == 0);
        if (!push) {
            const int prev = (mKBM_LeaderTrailHead + KBM_TrailMax - 1) % KBM_TrailMax;
            if (mKBM_LeaderTrail[prev].mX != lx || mKBM_LeaderTrail[prev].mY != ly)
                push = true;
        }
        if (push) {
            mKBM_LeaderTrail[mKBM_LeaderTrailHead].mX = lx;
            mKBM_LeaderTrail[mKBM_LeaderTrailHead].mY = ly;
            mKBM_LeaderTrailHead = (mKBM_LeaderTrailHead + 1) % KBM_TrailMax;
            if (mKBM_LeaderTrailCount < KBM_TrailMax)
                ++mKBM_LeaderTrailCount;
        }
    }

    if (dx == mKBM_LastDx && dy == mKBM_LastDy)
        return;

    if (NowIdle) {
        // Release: stop the squad cleanly, then place each follower at
        // its arc-length point on the recorded trail.
        sSprite** Members = mSquads[mSquad_Selected];

        // Collapse the walk queue to a single sentinel so every sprite,
        // upon reaching its mTarget, reads -1 and stays put.
        mSquad_WalkTargets[mSquad_Selected][0].asInt = -1;
        mSquad_Walk_Target_Indexes[mSquad_Selected] = 0;
        mSquad_Walk_Target_Steps[mSquad_Selected] = 0;

        const int Spacing = 8;
        const int Newest = (mKBM_LeaderTrailHead + KBM_TrailMax - 1) % KBM_TrailMax;

        for (int i = 0; Members && Members[i] != INVALID_SPRITE_PTR; ++i) {
            sSprite* Sprite = Members[i];
            Sprite->field_44 = 0;
            Sprite->mPosXFrac = 0;
            Sprite->mPosYFrac = 0;
            Sprite->mNextWalkTargetIndex = 0;

            if (i == 0 || Sprite->mInVehicle || mKBM_LeaderTrailCount == 0) {
                // Leader, vehicle passenger, or no trail: hold current
                // position as the stop point.
                Sprite->mTargetX = Sprite->mPosX;
                Sprite->mTargetY = Sprite->mPosY;
                continue;
            }

            // Walk the trail backwards, accumulating Euclidean arc
            // length, until we pass i*Spacing. Interpolate inside the
            // crossing segment to land at the exact distance.
            const int TargetArc = i * Spacing;
            int prevX = mKBM_LeaderTrail[Newest].mX;
            int prevY = mKBM_LeaderTrail[Newest].mY;
            int cum = 0;
            int16 placeX = prevX;
            int16 placeY = prevY;
            bool placed = false;
            for (int k = 1; k < mKBM_LeaderTrailCount; ++k) {
                const int idx = (Newest + KBM_TrailMax - k) % KBM_TrailMax;
                const int x = mKBM_LeaderTrail[idx].mX;
                const int y = mKBM_LeaderTrail[idx].mY;
                const int sx = x - prevX;
                const int sy = y - prevY;
                const int segLen = (int)std::sqrt((double)(sx * sx + sy * sy));
                if (segLen > 0) {
                    if (cum + segLen >= TargetArc) {
                        const int need = TargetArc - cum;
                        placeX = (int16)(prevX + (sx * need) / segLen);
                        placeY = (int16)(prevY + (sy * need) / segLen);
                        placed = true;
                        break;
                    }
                    cum += segLen;
                }
                prevX = x;
                prevY = y;
            }
            if (!placed) {
                // Trail shorter than needed — land at the oldest point.
                placeX = (int16)prevX;
                placeY = (int16)prevY;
            }

            // Walk the follower to its arc-length point rather than
            // teleporting — when the squad is spread out, snapping is
            // very visible. Shared mNext = 0 lets the anti-stack bump
            // fire naturally during transit if troops cross paths.
            Sprite->mFinishedWalking = 0;
            Sprite->mTargetX = placeX;
            Sprite->mTargetY = placeY;
        }

        // Note: do NOT clear mSquad_Join_TargetSquad here. Squad_Join_Check
        // only completes a merge while the sprite is moving (called from
        // Sprite_Draw_Row_Update); clearing it on every WASD release
        // would silently cancel a click-initiated join the moment the
        // player let go of WASD. Joins persist until completed or until
        // a new click reassigns them.

        mKBM_LastDx = dx;
        mKBM_LastDy = dy;
        return;
    }

    // Press/turn: 45°-preserved map-edge target
    int16 WalkX, WalkY;
    {
        const int32 mapW = mMapLoaded ? mMapLoaded->getWidthPixels()  : 0x4000;
        const int32 mapH = mMapLoaded ? mMapLoaded->getHeightPixels() : 0x4000;
        const int32 BIG  = mapW + mapH;
        int32 distX = BIG;
        if (dx < 0)      distX = mSquad_Leader->mPosX;
        else if (dx > 0) distX = mapW - 1 - mSquad_Leader->mPosX;
        int32 distY = BIG;
        if (dy < 0)      distY = mSquad_Leader->mPosY - 3;
        else if (dy > 0) distY = mapH - 1 - mSquad_Leader->mPosY;
        if (distX < 0) distX = 0;
        if (distY < 0) distY = 0;
        const int32 dist = (distX < distY) ? distX : distY;
        WalkX = (int16)(mSquad_Leader->mPosX + dx * dist);
        WalkY = (int16)(mSquad_Leader->mPosY + dy * dist);
    }

    if (WalkX < 0) WalkX = 0;
    if (WalkY < 3) WalkY = 3;

    // Same per-troop reset the mouse click performs before dispatching.
    for (auto& Troop : mGame_Data.mSoldiers_Allocated) {
        if (Troop.mSprite == INVALID_SPRITE_PTR || Troop.mSprite == 0)
            continue;
        if (mSquad_Selected != Troop.mSprite->field_32)
            continue;
        Troop.mSprite->field_44  = 0;
        Troop.mSprite->mPosXFrac = 0;
        Troop.mSprite->mPosYFrac = 0;
    }

    // Force chain rebuild so this acts like a fresh click — otherwise
    // an in-flight queue from a prior press would mean the leader
    // continues toward the old edge target before honouring this one.
    mSquad_Walk_Target_Steps[mSquad_Selected]   = 0;
    mSquad_Walk_Target_Indexes[mSquad_Selected] = 0;
    mSquad_WalkTargets[mSquad_Selected][0].asInt = -1;
    Squad_Walk_Target_Set(WalkX, WalkY, mSquad_Leader->field_32, mSquad_Leader->mPosX);

    // Same rationale as the release branch: leave mSquad_Join_TargetSquad
    // intact so a click-initiated merge can still complete while the
    // player steers with WASD.

    mKBM_LastDx = dx;
    mKBM_LastDy = dy;
}

// KBM vehicle mode: WASD drives, mouse aims, left-click fires. F key
// (handled via keyProcess → KBM_Vehicle_Enter_Or_Exit) exits.
void cFodder::Mouse_Inputs_Vehicle_KeyboardMouse() {
    sSprite* Vehicle = mSquad_CurrentVehicle;
    if (!Vehicle || Vehicle == INVALID_SPRITE_PTR)
        return;

    // Reticle cursor while driving, unless the cursor is parked in
    // the sidebar (Mouse_UpdateCursor already handles that case).
    if (mMouseX >= 32) {
        mMouseSpriteNew = eSprite_pStuff_Mouse_Target;
        mMouseX_Offset = -8;
        mMouseY_Offset = -8;
    }

    // Aim weapon continuously at mouse position (mirrors the
    // Vehicle_Target_Set / troop aim convention).
    int16 AimX = mMouseX + (mCameraX >> 16);
    int16 AimY = mMouseY + (mCameraY >> 16);
    AimX -= 0x10;
    if (!AimX) AimX = 1;
    AimY -= 8;
    Vehicle->mWeaponTargetX = AimX;
    Vehicle->mWeaponTargetY = AimY;

    // Left-click: fire vehicle weapon. Sprite_Handle_Vehicle's weapon
    // path reads mWeaponFireTimer, mirroring how Vehicle_Target_Set
    // triggers a shot in classic mode. Cursor in sidebar: let the
    // GUI loop have the click (e.g. weapon swap) instead of firing.
    const bool cursorInGameArea = (mMouseX >= 32);
    const bool leftJustPressed = cursorInGameArea && (mMouse_Button_Left_Toggle < 0);
    if (leftJustPressed) {
        Vehicle->mFiredWeaponType = -1;
        Vehicle->mWeaponFireTimer = -1;
    }

    // WASD steering: project a target point well ahead of the
    // vehicle in the WASD direction so Sprite_Handle_Vehicle's
    // turn-toward-target + speed ramp can play out naturally. When
    // WASD is released, park the target at the current position so
    // the vehicle decelerates and stops.
    int16 dx = 0;
    int16 dy = 0;
    if (mKey_A_Pressed) dx -= 1;
    if (mKey_D_Pressed) dx += 1;
    if (mKey_W_Pressed) dy -= 1;
    if (mKey_S_Pressed) dy += 1;

    if (dx == 0 && dy == 0) {
        Vehicle->mTargetX = Vehicle->mPosX;
        Vehicle->mTargetY = Vehicle->mPosY;
    } else {
        const int16 reach = 120;
        int16 TargetX = Vehicle->mPosX + dx * reach;
        int16 TargetY = Vehicle->mPosY + dy * reach;
        if (TargetX < 0) TargetX = 0;
        if (TargetY < 0x14) TargetY = 0x14;
        Vehicle->mTargetX = TargetX;
        Vehicle->mTargetY = TargetY;
    }

    // Keep the camera following the vehicle so the player can see
    // where they're driving.
    int16 CamX = Vehicle->mPosX - 0x1C;
    int16 CamY = Vehicle->mPosY + 6;
    if (CamX < 0) CamX = 0;
    if (CamY < 0x14) CamY = 0x14;
    mCamera_PanTargetX = CamX;
    mCamera_PanTargetY = CamY;
    mMouse_Locked = false;
}

// F-key handler: initiate entry if walking near a vehicle, or
// trigger exit if currently in one. Called from keyProcess on the
// key-down edge so a held F doesn't repeat.
void cFodder::KBM_Vehicle_Enter_Or_Exit() {
    if (!mKeyboardMouse_Mode) return;
    if (mSquad_Selected < 0) return;

    if (mSquad_CurrentVehicle) {
        Vehicle_Exit_KBM();
        return;
    }

    if (mSquad_Leader == INVALID_SPRITE_PTR || mSquad_Leader == 0)
        return;

    // Find the nearest enterable ground vehicle within range. Same
    // filters as Squad_Assign_Target_From_Mouse: human, enterable,
    // on the ground (height == 0).
    const int16 leaderX = mSquad_Leader->mPosX;
    const int16 leaderY = mSquad_Leader->mPosY - mSquad_Leader->mHeight;
    const int32 enterRadius = 60;
    const int32 maxD2 = enterRadius * enterRadius;

    sSprite* nearest = nullptr;
    int32    nearestD2 = maxD2 + 1;

    for (auto& Sprite : mSprites) {
        if (Sprite.mPosX == -32768) continue;
        if (!Sprite.mVehicleEnabled) continue;
        if (Sprite.mPersonType != eSprite_PersonType_Human) continue;
        if (Sprite.mHeight) continue;

        const int32 dx = Sprite.mPosX - leaderX;
        const int32 dy = Sprite.mPosY - leaderY;
        const int32 d2 = dx * dx + dy * dy;
        if (d2 > maxD2) continue;
        if (d2 < nearestD2) { nearestD2 = d2; nearest = &Sprite; }
    }

    if (!nearest) return;

    // Arm entry on every squad member: Sprite_Handle_Player_Enter_Vehicle
    // runs per-sprite each frame and completes the entry once a member
    // walks within 13px of the vehicle's entry point.
    sSprite** Members = mSquads[mSquad_Selected];
    for (;;) {
        if (*Members == INVALID_SPRITE_PTR) break;
        sSprite* Member = *Members++;
        Member->mVehicleWalkTarget = nearest;
    }

    // Also walk the squad toward the vehicle so they actually arrive.
    int16 WalkX = nearest->mPosX;
    int16 WalkY = nearest->mPosY - nearest->mHeight;
    if (WalkX < 0) WalkX = 0;
    if (WalkY < 3) WalkY = 3;

    for (auto& Troop : mGame_Data.mSoldiers_Allocated) {
        if (Troop.mSprite == INVALID_SPRITE_PTR || Troop.mSprite == 0)
            continue;
        if (mSquad_Selected != Troop.mSprite->field_32)
            continue;
        Troop.mSprite->field_44  = 0;
        Troop.mSprite->mPosXFrac = 0;
        Troop.mSprite->mPosYFrac = 0;
    }

    mSquad_Walk_Target_Steps[mSquad_Selected]   = 0;
    mSquad_Walk_Target_Indexes[mSquad_Selected] = 0;
    mSquad_WalkTargets[mSquad_Selected][0].asInt = -1;
    Squad_Walk_Target_Set(WalkX, WalkY,
        mSquad_Leader->field_32, mSquad_Leader->mPosX);

    mKBM_LeaderTrailHead  = 0;
    mKBM_LeaderTrailCount = 0;
    mKBM_LastDx = 0;
    mKBM_LastDy = 0;
}

void cFodder::Mouse_Inputs_Check() {
    int16 Data0 = 0;
    int16 Data4 = 0;

    if (mMouseDisabled)
        return;

    if (mPhase_In_Progress)
        Mouse_UpdateCursor();

    if (dword_3A030) {
        // TODO: Function pointer call, but appears not to be used
        //dword_3A030();
        return;
    }

    // Keyboard+mouse: run WASD / aim / merge / vehicle-drive BEFORE
    // the GUI loop so moving the cursor over the sidebar doesn't
    // freeze the squad mid-walk (the GUI loop's cursor-inside branch
    // returns early and would otherwise skip the KBM handler).
    // The handler is idempotent per interrupt tick — the post-GUI
    // call below then only consumes the click toggles for clicks
    // that landed in the game area.
    if (mKeyboardMouse_Mode && mPhase_In_Progress && mSquad_Selected >= 0)
        Mouse_Inputs_Check_KeyboardMouse();

    for (sGUI_Element* Loop_Element = mGUI_Elements;; ++Loop_Element) {

        if (Loop_Element->field_0 == 0)
            break;

        if ((*this.*Loop_Element->field_0)() < 0)
            return;

        Data0 = mGUI_Mouse_Modifier_X + Loop_Element->mX;

        int16 Data4 = mMouseX + 0x20;

        if (Data0 > Data4)
            continue;

        Data0 += Loop_Element->mWidth;
        if (Data0 < Data4)
            continue;

        Data0 = mGUI_Mouse_Modifier_Y;
        Data0 += Loop_Element->mY;
        if (Data0 > mMouseY)
            continue;

        Data0 += Loop_Element->mHeight;
        if (Data0 < mMouseY)
            continue;

        (*this.*Loop_Element->mMouseInsideFuncPtr)();
        return;
    }

    // Keyboard+mouse mode: replace classic click-to-move/right-click-to-aim with
    // continuous mouse aim + keyboard movement + click-to-fire.
    if (mKeyboardMouse_Mode && mPhase_In_Progress && mSquad_Selected >= 0) {
        Mouse_Inputs_Check_KeyboardMouse();
        Mouse_Button_Left_Toggled();
        Mouse_Button_Right_Toggled();
        return;
    }

    if (!mButtonPressRight)
        goto loc_30814;

    if (mSquad_Selected < 0)
        return;

    if (mMouseSpriteNew < 0) {
        mMouseSpriteNew = eSprite_pStuff_Mouse_Target;
        mMouseX_Offset = -8;
        mMouseY_Offset = -8;
    }
    Squad_Member_Target_Set();
    if (!mSquad_CurrentVehicle)
        return;

loc_30814:;

    if (mSquad_CurrentVehicle) {
        Vehicle_Input_Handle();
        return;
    }

    if (Mouse_Button_Left_Toggled() < 0)
        return;

    if (mMouse_Button_LeftRight_Toggle2)
        return;

    if (mMouseSpriteNew < 0) {
        mMouseSpriteNew = eSprite_pStuff_Mouse_Cursor;
        mMouseX_Offset = 0;
        mMouseY_Offset = 0;
    }

    if (mSquad_Selected < 0) {

        Squad_Member_Click_Check();
        return;
    }

    sSprite** Data24 = mSquads[mSquad_Selected];
    sSprite* Dataa24 = *Data24;

    if (Dataa24 == INVALID_SPRITE_PTR) {
        Squad_Member_Click_Check();
        return;
    }

    if (Dataa24->mInVehicle)
        return;

    if (word_3B2F1)
        word_3B2F1 = 0;
    else
        Squad_Assign_Target_From_Mouse();

    mSquad_Member_Clicked_TroopInSameSquad = 0;
    Squad_Member_Click_Check();

    // If we clicked on a member of the current squad, nothing to do
    if (mSquad_Member_Clicked_TroopInSameSquad)
        return;

    if (mSquad_Leader == INVALID_SPRITE_PTR || mSquad_Leader == 0)
        return;

    Data0 = mMouseX + (mCameraX >> 16);
    Data4 = mMouseY + (mCameraY >> 16);

    Data0 += -22;
    if (Data0 < 0)
        Data0 = 0;

    Data4 += -3;
    if (Data4 < 0)
        Data4 = 3;

    if (Data4 < 3)
        Data4 = 3;

    mCamera_PanTargetX = Data0;
    mCamera_PanTargetY = Data4;

    mMouse_Locked = false;

    for (auto& Troop : mGame_Data.mSoldiers_Allocated) {

        if (Troop.mSprite == INVALID_SPRITE_PTR || Troop.mSprite == 0)
            continue;

        // Not in the squad?
        if (mSquad_Selected != Troop.mSprite->field_32)
            continue;

        Troop.mSprite->field_44 = 0;
        Troop.mSprite->mPosXFrac = 0;
        Troop.mSprite->mPosYFrac = 0;
    }

    int16 Data10 = mSquad_Leader->mPosX;
    int16 Data8 = Data4;
    Data4 = Data0;

    Squad_Walk_Target_Set(Data4, Data8, mSquad_Leader->field_32, Data10);

    // Reset the join target
    for (auto& JoinTargetSquad : mSquad_Join_TargetSquad) {

        if (mSquad_Selected != JoinTargetSquad)
            continue;

        JoinTargetSquad = -1;
    }
}

int16 cFodder::Mouse_Button_Right_Toggled() {

    if (mMouse_Button_Right_Toggle < 0) {
        mMouse_Button_Right_Toggle = 1;
        return 1;
    }

    return -1;
}
