#include <algorithm>
#include <assert.h>
#include "Utils.h"

#include "Mario.h"
#include "Game.h"

#include "Goomba.h"
#include "Portal.h"

CMario::CMario(float x, float y) : CGameObject()
{
	level = MARIO_LEVEL_BIG;
	untouchable = 0;
	SetState(MARIO_STATE_IDLE); 

	start_x = x;
	start_y = y;
	this->x = x;
	this->y = y;
}

void CMario::Update(DWORD dt, vector<LPGAMEOBJECT>* coObjects)
{
	// Calculate dx, dy 
	CGameObject::Update(dt);

	// Simple fall down
	vy += MARIO_GRAVITY * dt;

	vector<LPCOLLISIONEVENT> coEvents;
	vector<LPCOLLISIONEVENT> coEventsResult;

	coEvents.clear();

	// turn off collision when die 
	if (state != MARIO_STATE_DIE)
		CalcPotentialCollisions(coObjects, coEvents);
	
	// reset untouchable timer if untouchable time has passed
	if (GetTickCount() - untouchable_start > MARIO_UNTOUCHABLE_TIME)
	{
		untouchable_start = 0;
		untouchable = 0;
	}

	// No collision occured, proceed normally
	if (coEvents.size() == 0)
	{
		x += dx;
		y += dy;
	}
	else
	{
		float min_tx, min_ty, nx = 0, ny;
		float rdx = 0;
		float rdy = 0;

		// TODO: This is a very ugly designed function!!!!
		FilterCollision(coEvents, coEventsResult, min_tx, min_ty, nx, ny, rdx, rdy);

		// how to push back Mario if collides with a moving objects, what if Mario is pushed this way into another object?
		//if (rdx != 0 && rdx!=dx)
		//	x += nx*abs(rdx); 

		// block every object first!
		x += min_tx * dx + nx * 0.4f;
		y += min_ty * dy + ny * 0.4f;

		if (nx != 0) vx = last_vx;
		if (ny != 0) vy = 9999999;
		
		if (vy == 9999999) isJumping = false;
		//
		// Collision logic with other objects
		//
		for (UINT i = 0; i < coEventsResult.size(); i++)
		{
			LPCOLLISIONEVENT e = coEventsResult[i];

  			if (dynamic_cast<CGoomba*>(e->obj)) // if e->obj is Goomba 
			{
				CGoomba* goomba = dynamic_cast<CGoomba*>(e->obj);

				// jump on top >> kill Goomba and deflect a bit 
				if (e->ny < 0)
				{
					if (goomba->GetState() != GOOMBA_STATE_DIE)
					{
						goomba->SetState(GOOMBA_STATE_DIE);
						vy = -MARIO_JUMP_DEFLECT_SPEED;
					}
				}
				else if (e->nx != 0)
				{
					if (untouchable == 0)
					{
						if (goomba->GetState() != GOOMBA_STATE_DIE)
						{
							if (level > MARIO_LEVEL_SMALL)
							{
								level = MARIO_LEVEL_SMALL;
								StartUntouchable();
							}
							else
								SetState(MARIO_STATE_DIE);
						}
					}
				}
			} // if Goomba
			else if (dynamic_cast<CPortal*>(e->obj))
			{
				CPortal* p = dynamic_cast<CPortal*>(e->obj);
				CGame::GetInstance()->SwitchScene(p->GetSceneId());
			}
		}
	}

	// clean up collision events
	for (UINT i = 0; i < coEvents.size(); i++) delete coEvents[i];
}

void CMario::Render()
{
	CMarioGeneral* mario_general = new CMarioGeneral();
	//mario_general = CMarioGeneral::GetInstance();
	mario_general->LoadListAni();
	int ani = -1;
	if (state == MARIO_STATE_DIE)
		ani = MARIO_ANI_DIE;
	else
		switch (level)
		{
		case MARIO_LEVEL_BIG:
			mario_general->SetLevel(MARIO_LEVEL_BIG);
			ani = RenderFromAniGroup(mario_general);
			break;
		case MARIO_LEVEL_SMALL:
			mario_general->SetLevel(MARIO_LEVEL_SMALL);
			ani = RenderFromAniGroup(mario_general);
			break;
		}

	int alpha = 255;
	if (untouchable) alpha = 128;
	/*if (speed_Up) {
		animation_set->at(ani)->SetHightSpeed();
	}*/
	animation_set->at(ani)->Render(x, y, alpha);

	RenderBoundingBox();
}



int CMario::RenderFromAniGroup(CMarioGeneral* anigroup)
{
	int aniIndex = -1;
	int ani = -1;
	//DebugOut(L"vx %d", nx);
	//Mario Stop
	if (vx == 0)
	{
		//Ngoi
		if ((isSitting) && (!isJumping))					
		{
			if (nx < 0) aniIndex = MARIO_ANI_SITTING_LEFT;
			else
				aniIndex = MARIO_ANI_SITTING_RIGHT;
		}
		//Dung yen
		else
		{
			if (nx < 0) aniIndex = MARIO_ANI_IDLE_LEFT;
			else
				aniIndex = MARIO_ANI_IDLE_RIGHT;
		}
	}
	else if (nx == 1)
	{
		if ((isSitting)&&(!isJumping))
		{
			aniIndex = MARIO_ANI_SITTING_RIGHT;
		}
		else
			aniIndex = MARIO_ANI_WALKING_RIGHT;
	}
	else if (nx == -1)
	{
		if ((isSitting) && (!isJumping))
		{
			 aniIndex = MARIO_ANI_SITTING_LEFT;	
		}
		else
		aniIndex = MARIO_ANI_WALKING_LEFT;

	}
	

	ani = anigroup->GetAni_Mario(aniIndex);
	if (isSpeedUp)
	{
		animation_set->at(ani)->SetHightSpeed();
	}
	return ani;
}

void CMario::DecreaseSpeed(float speedDown)
{
	float ACCELERATOR;
	if (isSpeedUp)
		ACCELERATOR = MARIO_RUNNING_DECELERATION;
	else
		ACCELERATOR = MARIO_WALKING_DECELERATION;
	if (vx > speedDown)
	{
		vx -= ACCELERATOR * dt;
		if (vx <= speedDown)
			vx = speedDown;
	}
	else
	{
		vx += ACCELERATOR * dt;
		if (vx >= speedDown)
			vx = speedDown;
	}
	last_vx = vx;
}

void CMario::SetState(int state)
{
	CGameObject::SetState(state);

	switch (state)
	{
	case MARIO_STATE_WALKING:
		if (nx == 1)
		{
			// Neu toc do cao hon di bo -> giam toc do 
			while (vx > MARIO_WALKING_MAX_SPEED)
			{
				DecreaseSpeed(MARIO_WALKING_MAX_SPEED);
				DebugOut(L"\nMario Speed Down: vx: %f", vx);
			}
				
			vx += MARIO_WALKING_ACCELERATION * dt;
			if (vx >= MARIO_WALKING_MAX_SPEED)
				vx = MARIO_WALKING_MAX_SPEED;
			DebugOut(L"\nMario Speed Up: vx: %f", vx);
			break;
		}
		else if (nx == -1)
		{
			while (vx < -MARIO_WALKING_MAX_SPEED)
			{
				DecreaseSpeed(-MARIO_WALKING_MAX_SPEED);
				DebugOut(L"\nMario Speed Down: vx: %f", vx);
			}

			vx -= MARIO_WALKING_ACCELERATION * dt;
			if (vx <= -MARIO_WALKING_MAX_SPEED)
				vx = -MARIO_WALKING_MAX_SPEED;
			DebugOut(L"\nMario Speed Up: vx: %f", vx);
			break;
		}		
	case MARIO_STATE_SITTING:
		DecreaseSpeed(0);
		//DebugOut(L"\nToc do giam khi Ngoi vx: %f", vx); OKER
		break;
	case MARIO_STATE_RUNNING:
		if (nx == 1)
		{
			vx += MARIO_RUNNING_ACCELERATION * dt;
			if (vx >= MARIO_RUNNING_MAX_SPEED)
				vx = MARIO_RUNNING_MAX_SPEED;
			break;
		}
		else
		{
			vx -= MARIO_RUNNING_ACCELERATION * dt;
			if (vx <= -MARIO_RUNNING_MAX_SPEED)
				vx = -MARIO_RUNNING_MAX_SPEED;
			break;
		}
	case MARIO_STATE_IDLE:
		DecreaseSpeed(0);
		break;
	}
}

void CMario::GetBoundingBox(float& left, float& top, float& right, float& bottom)
{
	left = x;
	top = y;

	if (level == MARIO_LEVEL_BIG)
	{
		right = x + MARIO_BIG_BBOX_WIDTH;
		bottom = y + MARIO_BIG_BBOX_HEIGHT;
	}
	else
	{
		right = x + MARIO_SMALL_BBOX_WIDTH;
		bottom = y + MARIO_SMALL_BBOX_HEIGHT;
	}
}

/*
	Reset Mario status to the beginning state of a scene
*/
void CMario::Reset()
{
	SetState(MARIO_STATE_IDLE);
	SetLevel(MARIO_LEVEL_BIG);
	SetPosition(start_x, start_y);
	SetSpeed(0, 0);
}

void CMario::Go()
{
	//DebugOut(L"Trang thai tang toc: %b", isSpeedUp);
	if (isSpeedUp)
	{
		SetState(MARIO_STATE_RUNNING);
	}
	else
	if ((!isJumping) && (!isSitting))
	{
		SetState(MARIO_STATE_WALKING);
	}
	//DebugOut(L"\nToc do mario vx: %f", vx);
	last_vx = vx;
}


void CMario::Left()
{
	isSitting = false;
	nx = -1;
}

void CMario::Right()
{
	isSitting = false;
	nx = 1;
}

void CMario::Jump()
{
	isJumping = true;
}

void CMario::Sit()
{
	isSitting = true;
	SetState(MARIO_STATE_SITTING);
}

void CMario::Idle()
{
	isSitting = false;
	SetState(MARIO_STATE_IDLE);
}

