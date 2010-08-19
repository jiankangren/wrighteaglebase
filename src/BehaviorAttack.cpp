/************************************************************************************
 * WrightEagle (Soccer Simulation League 2D)                                        *
 * BASE SOURCE CODE RELEASE 2010                                                    *
 * Copyright (C) 1998-2010 WrightEagle 2D Soccer Simulation Team,                   *
 *                         Multi-Agent Systems Lab.,                                *
 *                         School of Computer Science and Technology,               *
 *                         University of Science and Technology of China, China.    *
 *                                                                                  *
 * This program is free software; you can redistribute it and/or                    *
 * modify it under the terms of the GNU General Public License                      *
 * as published by the Free Software Foundation; either version 2                   *
 * of the License, or (at your option) any later version.                           *
 *                                                                                  *
 * This program is distributed in the hope that it will be useful,                  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of                   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                    *
 * GNU General Public License for more details.                                     *
 *                                                                                  *
 * You should have received a copy of the GNU General Public License                *
 * along with this program; if not, write to the Free Software                      *
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.  *
 ************************************************************************************/

#include "BehaviorAttack.h"
#include "BehaviorShoot.h"
#include "BehaviorPass.h"
#include "BehaviorDribble.h"
#include "BehaviorIntercept.h"
#include "BehaviorPosition.h"
#include "WorldState.h"


BehaviorAttackPlanner::BehaviorAttackPlanner(Agent & agent): BehaviorPlannerBase<BehaviorAttackData>( agent )
{
}

BehaviorAttackPlanner::~BehaviorAttackPlanner()
{
}

void BehaviorAttackPlanner::Plan(std::list<ActiveBehavior> & behavior_list)
{
	BehaviorInterceptPlanner(mAgent).Plan(mActiveBehaviorList);
	BehaviorShootPlanner(mAgent).Plan(mActiveBehaviorList);
	BehaviorPassPlanner(mAgent).Plan(mActiveBehaviorList);
	BehaviorDribblePlanner(mAgent).Plan(mActiveBehaviorList);
	BehaviorPositionPlanner(mAgent).Plan(mActiveBehaviorList);

	if (!mActiveBehaviorList.empty()) {
		mActiveBehaviorList.sort(std::greater<ActiveBehavior>());
		behavior_list.push_back(mActiveBehaviorList.front());

		if (mActiveBehaviorList.size() > 1) { //允许非最优行为提交视觉请求
			double plus = 1.0;
			ActiveBehaviorPtr it = mActiveBehaviorList.begin();
			for (++it; it != mActiveBehaviorList.end(); ++it) {
				it->SubmitVisualRequest(plus);
				plus *= 2.0;
			}
		}
	}
}
