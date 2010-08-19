#include "VisualSystem.h"
#include "Strategy.h"
#include "Formation.h"
#include "TimeTest.h"
#include "PositionInfo.h"
#include "InterceptInfo.h"
#include "InterceptModel.h"
#include "Agent.h"
#include "BehaviorShoot.h"
#include "Logger.h"

VisualSystem::VisualSystem()
{
	mCanForceChangeViewWidth = false;
	mIsSearching = false;
	mIsCritical = false;
	mForbidden = false;
	mForceToSeeObject.bzero();
}

VisualSystem::~VisualSystem() {
}

VisualSystem & VisualSystem::instance()
{
	static VisualSystem info_system;
	return info_system;
}

void VisualSystem::Initial(Agent * agent)
{
	mpAgent         = agent;
	mpWorldState    = & (agent->World());
	mpInfoState     = & (agent->Info());
	mpBallState     = & (agent->World().Ball());
	mpSelfState     = & (agent->Self());

	mVisualRequest.GetOfBall().mpObject = mpBallState;
	mVisualRequest.GetOfBall().mUnum = 0;

	for (Unum i = 1; i <= TEAMSIZE; ++i) {
		mVisualRequest.GetOfTeammate(i).mpObject = & (mpWorldState->GetTeammate(i));
		mVisualRequest.GetOfOpponent(i).mpObject = & (mpWorldState->GetOpponent(i));
		mVisualRequest.GetOfTeammate(i).mUnum = i;
		mVisualRequest.GetOfOpponent(i).mUnum = -i;
	}
}

void VisualSystem::ResetVisualRequest()
{
	mCanTurn = false;
	mSenseBallCycle = 0;

	if (mpAgent->IsNewSight()) { //�����Ӿ������ã�����ά�������ڵĽ��
		mCanForceChangeViewWidth = false;
		mIsSearching = false;
		mIsCritical = false;
		mForbidden = false;

		for (Unum i = -TEAMSIZE; i <= TEAMSIZE; ++i){
			mVisualRequest[i].Clear();
		}

		mHighPriorityPlayerSet.clear();
		mForceToSeeObject.bzero();
	}

	if (mpSelfState->GetPosConf() < FLOAT_EPS) {
		mIsCritical = true;
		mCanForceChangeViewWidth = false;
	}
	if (mpBallState->GetPosConf() < FLOAT_EPS) {
		RaiseForgotObject(0);
	}

	ViewModeDecision();
}

void VisualSystem::Decision()
{
	if (!mpAgent->GetBeliefState()) return;
	if (mpAgent->GetActionEffector().IsTurnNeck()) return; //�����ط��Ѿ�������ת���Ӷ���
	if (mForbidden) return;

	if (!DealWithSetPlayMode()) {
		DoDecision();
	}
}

void VisualSystem::ViewModeDecision()
{
	ChangeViewWidth(mpSelfState->GetViewWidth());

	if (mpAgent->IsNewSight() == false){
		ChangeViewWidth(mpSelfState->GetViewWidth());
		return;
	}

	if (mpWorldState->GetPlayMode() != PM_Before_Kick_Off){
		double balldis  = mpInfoState->GetPositionInfo().GetBallDistToTeammate(mpSelfState->GetUnum());
		if (balldis > 60.0){
			ChangeViewWidth(VW_Wide);
		}
		else if (balldis > 40.0){
			ChangeViewWidth(VW_Normal);
		}
		else {
			ChangeViewWidth(VW_Narrow);
		}
	}
	else{
		ChangeViewWidth(VW_Narrow);
	}
}

void VisualSystem::DealVisualRequest()
{
	DealWithSpecialObjects();
	SetVisualRing();
	GetBestVisualAction();
}

void VisualSystem::EvaluateVisualRequest()
{
	{
		VisualRequest *vr = &mVisualRequest.GetOfBall();
		vr->mValid = !mpAgent->GetBeliefState()->GetAppearanceSet(0).empty() || mpBallState->GetPosConf() > FLOAT_EPS;

		if (vr->mValid) {
			vr->mPreDistance = (mPreBallPos - mPreSelfPos).Mod();
			vr->mCycleDelay = mpBallState->GetPosDelay();

			vr->UpdateEvaluation();

			if (vr->mConf < FLOAT_EPS){
				mForceToSeeObject.GetOfBall() = true;
			}
		}
	}

	PlayMode play_mode = mpWorldState->GetPlayMode();

	if (play_mode == PM_Our_Penalty_Ready || play_mode == PM_Our_Penalty_Taken) {
		const int goalie = mpWorldState->GetOpponentGoalieUnum();
		if (!goalie) return;

		VisualRequest *vr = &mVisualRequest.GetOfOpponent(goalie);

		if (mpWorldState->GetOpponent(goalie).IsAlive()){
			Vector pos = mpWorldState->Opponent(goalie).GetPredictedPos(1) - mPreSelfPos;
			vr->mValid = true;
			vr->mPreDistance = pos.Mod();
			vr->mCycleDelay = vr->mpObject->GetPosDelay();

			vr->UpdateEvaluation();

			if (vr->mConf < FLOAT_EPS) {
				mHighPriorityPlayerSet.insert(vr);
			}
		}
		else {
			vr->mValid = false;
		}
	}
	else {
		for (int i = -TEAMSIZE; i <= TEAMSIZE; ++i) {
			if (!i || i == mpSelfState->GetUnum()) continue;

			VisualRequest *vr = &mVisualRequest[i];
			vr->mValid = !mpAgent->GetBeliefState()->GetAppearanceSet(i).empty() || mpWorldState->GetPlayer(i).IsAlive();

			if (vr->mValid) {
				Vector pre_rel_pos;

				if (mpWorldState->GetPlayer(i).IsAlive()) {
					pre_rel_pos = mpWorldState->GetPlayer(i).GetPredictedPos() - mPreSelfPos;
				}
				else {
					pre_rel_pos = mpAgent->GetBeliefState()->GetExpectedPos(i) - mPreSelfPos;
				}

				vr->mPreDistance = pre_rel_pos.Mod();
				vr->mCycleDelay = vr->mpObject->GetPosDelay();

				vr->UpdateEvaluation();

				if (vr->mConf < FLOAT_EPS){
					mHighPriorityPlayerSet.insert(vr);
				}
			}
		}
	}
}

void VisualSystem::DoInfoGather()
{
	const Strategy & strategy = mpAgent->GetStrategy();

	if (!mCanTurn){
		if (mpAgent->GetActionEffector().IsMutex() == false && !mIsCritical){
			SetCanTurn(true);
		}
	}
	else {
		if (mpAgent->GetActionEffector().IsMutex() || mIsCritical){
			SetCanTurn(false);
		}
	}

	if (mpSelfState->IsIdling()) {
		SetCanTurn(false);
	}

	UpdatePredictInfo();

	if(mpWorldState->GetPlayMode() == PM_Our_Penalty_Ready || mpWorldState->GetPlayMode() == PM_Our_Penalty_Taken /*|| mpWorldState->GetPlayMode() == PM_Our_Penalty_Setup*/ ){
		RaisePlayer(-mpWorldState->GetOpponentGoalieUnum(), 1.0);
		RaiseBall();
		return;
	}

	if (mpWorldState->GetPlayMode() > PM_Opp_Mode && mpInfoState->GetPositionInfo().GetClosestOpponentDistToBall() < 3.0 && mpBallState->GetPos().Dist(mpSelfState->GetPos()) < 20.0 && mpBallState->GetPosDelay() > 1){
		SetForceSeeBall(); //��ע���ַ���
	}

	if(mpSelfState->GetPos().X() - mpInfoState->GetPositionInfo().GetTeammateOffsideLine() > -5.0 && mpInfoState->GetPositionInfo().GetTeammateOffsideLineOpp() != Unum_Unknown){//offside look
		RaisePlayer(-mpInfoState->GetPositionInfo().GetTeammateOffsideLineOpp(), 2.0);
	}

	if (mpSelfState->IsGoalie()){
		if (strategy.IsMyControl() || mpWorldState->GetPlayMode() == PM_Our_Goal_Kick){
			DoInfoGatherForDefense();
		}
		else {
			DoInfoGatherForGoalie();
		}
	}
	else {
		if (strategy.IsBallFree()
				&& strategy.GetController() != 0
				&& mpAgent->GetFormation().GetTeammateRoleType(mpSelfState->GetUnum()).mLineType != LT_Defender
				&& (strategy.GetBallFreeCycleLeft() > 3 || (strategy.IsMyControl() && strategy.GetMyInterCycle() > 3)))
		{
			DoInfoGatherForBallFree();
		}
		else {
			switch(strategy.GetSituation()){
			case ST_Defense:
				DoInfoGatherForDefense();
				break;
			case ST_Forward_Attack:
				DoInfoGatherForFastForward();
				break;
			case ST_Penalty_Attack:
				DoInfoGatherForPenaltyAttack();
				break;
			}
		}
	}

	if (mpWorldState->GetPlayMode() != PM_Play_On) {
		DoInfoGatherForBallFree();
	}
}

void VisualSystem::DoInfoGatherForBallFree()
{
	const Strategy & strategy = mpAgent->GetStrategy();

	double ball_free_cyc_left; //ball free cycle left,�����Ǿ����������и��������freecyc,��ͬ��StrategyInfo����Ǹ�ֵ
	const double rate = 0.6; //������Ϊ�ǵ���ֵ������ʵֵ�ĳɹ���
	double my_int_cycle, tm_int_cycle, opp_int_cycle;

	my_int_cycle = strategy.GetMyInterCycle();
	tm_int_cycle = strategy.GetMinTmInterCycle() * rate + strategy.GetSureTmInterCycle() * (1 - rate);
	opp_int_cycle = strategy.GetMinOppInterCycle() * rate + strategy.GetSureOppInterCycle() * (1 - rate);

	ball_free_cyc_left = Min(tm_int_cycle, opp_int_cycle);
	ball_free_cyc_left = Min(my_int_cycle, ball_free_cyc_left);

	if(opp_int_cycle < ball_free_cyc_left + 2 && ball_free_cyc_left > 3){
		ball_free_cyc_left -= 1;
	}

	std::vector<OrderedIT>::const_iterator it;
	const std::vector<OrderedIT> & OIT = mpInfoState->GetInterceptInfo().GetOIT();

	if (strategy.IsMyControl() && mpAgent->GetFormation().GetTeammateRoleType(mpSelfState->GetUnum()).mLineType != LT_Defender && ball_free_cyc_left < 6 && ball_free_cyc_left > 2){
		RaiseBall(1.0);
	}
	else{
		RaiseBall();
	}

	if(!strategy.IsMyControl()){//�Լ���������
		int eva = 3;
		for (it = OIT.begin(); it != OIT.end(); ++it){
			if(it->mpInterceptInfo->mMinCycle > 50) break;
			RaisePlayer(it->mUnum, eva++);
		}
	}
	else {//�Լ���Ҫ����
		RaiseBall(2.0);
		int eva = 3;
		for (it = OIT.begin(); it != OIT.end(); ++it){
			if(it->mpInterceptInfo->mMinCycle > 50) break;
			if (it->mUnum == mpSelfState->GetUnum()) continue;
			RaisePlayer(it->mUnum, ++eva);
			if (mpWorldState->GetPlayer(it->mUnum).GetPosDelay() > it->mpInterceptInfo->mMinCycle) {
				mHighPriorityPlayerSet.insert(GetVisualRequest(it->mUnum));
			}
		}
	}
}

void VisualSystem::DoInfoGatherForFastForward()
{
	const Strategy & strategy = mpAgent->GetStrategy();

	RaiseBall();
	switch (mpAgent->GetFormation().GetTeammateRoleType(mpSelfState->GetUnum()).mLineType){
	case LT_Defender:
		for (Unum i = 1; i <= TEAMSIZE; ++i){
			if (i != mpSelfState->GetUnum() && i != mpWorldState->GetTeammateGoalieUnum()){
				switch (mpAgent->GetFormation().GetTeammateRoleType(i).mLineType){
				case LT_Defender:
					RaisePlayer(i, 8.0);
					break;
				case LT_Midfielder:
					RaisePlayer(i, 5.0);
					break;
				case LT_Forward:
					RaisePlayer(i, 5.0);
					break;
				default:
					PRINT_ERROR("line type error");
					break;
				}
			}
		}
		break;
	case LT_Midfielder:
		for (Unum i = 1; i <= TEAMSIZE; ++i){
			if (i != mpSelfState->GetUnum() && i != mpWorldState->GetTeammateGoalieUnum()){
				switch (mpAgent->GetFormation().GetTeammateRoleType(i).mLineType){
				case LT_Defender:
					RaisePlayer(i, 100.0);
					break;
				case LT_Midfielder:
					RaisePlayer(i, 5.0);
					break;
				case LT_Forward:
					RaisePlayer(i, 2.0);
					break;
				default:
					PRINT_ERROR("line type error");
					break;
				}
			}
			if (mpWorldState->GetOpponent(i).GetPosConf() > FLOAT_EPS){
				if(mpWorldState->GetOpponent(i).GetPos().X() > mPreBallPos.X() - 8.0)
					RaisePlayer(-i, 4.0);
				else
					RaisePlayer(-i, 100.0);
			}
		}
		if (mpWorldState->GetOpponentGoalieUnum() && mpWorldState->GetOpponent(mpWorldState->GetOpponentGoalieUnum()).GetPosConf() > FLOAT_EPS){
			RaisePlayer(-mpWorldState->GetOpponentGoalieUnum(), 20.0);
		}
		break;
	case LT_Forward:
		for (Unum i = 1; i <= TEAMSIZE; ++i){
			if (i != mpSelfState->GetUnum() && i != mpWorldState->GetTeammateGoalieUnum()){
				switch (mpAgent->GetFormation().GetTeammateRoleType(i).mLineType){
				case LT_Defender:
					RaisePlayer(i, 100.0);
					break;
				case LT_Midfielder:
					RaisePlayer(i, 5.0);
					break;
				case LT_Forward:
					RaisePlayer(i, 3.0);
					break;
				default:
					PRINT_ERROR("line type error");
					break;
				}
			}
			if (mpWorldState->GetOpponent(i).GetPosConf() > FLOAT_EPS){
				double opp_x = mpWorldState->GetOpponent(i).GetPos().X();
				if (!strategy.IsMyControl() && fabs(opp_x - mpInfoState->GetPositionInfo().GetTeammateOffsideLine()) < 1.0){
					RaisePlayer(-i, 2.6);
				}
				else if (!strategy.IsMyControl() && fabs(opp_x - mpInfoState->GetPositionInfo().GetTeammateOffsideLine()) < 3.6){
					RaisePlayer(-i, 3);
				}
				else if (opp_x > mPreBallPos.X() - 8.0){
					if (opp_x > mPreBallPos.X() + 36.0){
						RaisePlayer(-i, 6);
					}
					else{
						RaisePlayer(-i, 4);
					}
				}
				else{
					RaisePlayer(-i, 100);
				}
			}
		}
		if (mpWorldState->GetOpponentGoalieUnum() && mpWorldState->GetOpponent(mpWorldState->GetOpponentGoalieUnum()).GetPosConf() > FLOAT_EPS){
			RaisePlayer(-mpWorldState->GetOpponentGoalieUnum(), 15.0);
		}
		break;
	default:
		PRINT_ERROR("line type error");
		break;
	}
}

void VisualSystem::DoInfoGatherForPenaltyAttack()
{
	const Strategy & strategy = mpAgent->GetStrategy();

	RaiseBall();
	switch (mpAgent->GetFormation().GetTeammateRoleType(mpSelfState->GetUnum()).mLineType){
	case LT_Defender:
		for (Unum i = 1; i <= TEAMSIZE; ++i){
			if (i != mpSelfState->GetUnum() && i != mpWorldState->GetTeammateGoalieUnum()){
				RaisePlayer(i);
			}
			RaisePlayer(-i);
		}
		break;
	case LT_Midfielder:
		for (Unum i = 1; i <= TEAMSIZE; ++i){
			if (i != mpSelfState->GetUnum() && i != mpWorldState->GetTeammateGoalieUnum()){
				switch (mpAgent->GetFormation().GetTeammateRoleType(i).mLineType){
				case LT_Defender:
					RaisePlayer(i, 100.0);
					break;
				case LT_Midfielder:
					RaisePlayer(i, 5.0);
					break;
				case LT_Forward:
					RaisePlayer(i, 5.0);
					break;
				default:
					PRINT_ERROR("line type error");
					break;
				}
			}
			if (mpWorldState->GetOpponent(i).GetPosConf() > FLOAT_EPS){
				if(mpWorldState->GetOpponent(i).GetPos().X() > mPreBallPos.X() - 8.0)
					RaisePlayer(-i, 3.0);
				else
					RaisePlayer(-i, 100.0);
			}
		}
		if (mpWorldState->GetOpponentGoalieUnum() && mpWorldState->GetOpponent(mpWorldState->GetOpponentGoalieUnum()).GetPosConf() > FLOAT_EPS){
			RaisePlayer(-mpWorldState->GetOpponentGoalieUnum(), 20.0);
		}
		break;
	case LT_Forward:
		for (Unum i = 1; i <= TEAMSIZE; ++i){
			if (i != mpSelfState->GetUnum() && i != mpWorldState->GetTeammateGoalieUnum()){
				switch (mpAgent->GetFormation().GetTeammateRoleType(i).mLineType){
				case LT_Defender:
					RaisePlayer(i, 100.0);
					break;
				case LT_Midfielder:
					RaisePlayer(i, 5.0);
					break;
				case LT_Forward:
					RaisePlayer(i, 3.0);
					break;
				default:
					PRINT_ERROR("line type error");
					break;
				}
			}
			if (mpWorldState->GetOpponent(i).GetPosConf() > FLOAT_EPS){
				if(mpWorldState->GetOpponent(i).GetPos().X() > mPreBallPos.X() - 8.0)
					RaisePlayer(-i, 3.0);
				else
					RaisePlayer(-i, 100.0);
			}
		}
		break;
	default:
		PRINT_ERROR("line type error");
		break;
	}

	//�����Ӿ�����
	if(ServerParam::instance().oppPenaltyArea().IsWithin(mPreSelfPos)
			&& (strategy.IsMyControl()
					|| (strategy.IsTmControl() && mpInfoState->GetPositionInfo().GetBallDistToTeammate(mpSelfState->GetUnum()) < 20.0))){
		if (mpWorldState->GetOpponentGoalieUnum() && mpWorldState->GetOpponent(mpWorldState->GetOpponentGoalieUnum()).GetPosConf() > FLOAT_EPS){
			if (strategy.IsMyControl()
					&& (mpSelfState->GetPos().X() > 38.0 || (mpInfoState->GetPositionInfo().GetBallDistToOpponent(mpWorldState->GetOpponentGoalieUnum()) < 8.0 && mpSelfState->GetPos().X() > 36.0)))
			{
				if (mpWorldState->GetPlayMode() == PM_Our_Back_Pass_Kick || mpWorldState->GetPlayMode() == PM_Our_Indirect_Free_Kick){
					RaisePlayer(-mpWorldState->GetOpponentGoalieUnum(), 1.2);
				}
				else {
					RaisePlayer(-mpWorldState->GetOpponentGoalieUnum(), 1.0);
				}
			}
			else {
				RaisePlayer(-mpWorldState->GetOpponentGoalieUnum(), 2.0);
			}
		}
	}
}

void VisualSystem::DoInfoGatherForDefense()
{
	if(mVisualRequest.GetOfBall().mConf < FLOAT_EPS && !mCanTurn && !mIsCritical){
		mForceToSeeObject.GetOfBall() = true;
	}

	RaiseBall();
	switch (mpAgent->GetFormation().GetTeammateRoleType(mpSelfState->GetUnum()).mLineType){
	case LT_Goalie:
	case LT_Defender:
		for (Unum i = 1; i <= TEAMSIZE; ++i){
			if (i != mpSelfState->GetUnum() && i != mpWorldState->GetTeammateGoalieUnum()){
				switch (mpAgent->GetFormation().GetTeammateRoleType(i).mLineType){
				case LT_Defender:
					RaisePlayer(i, 5.0);
					break;
				case LT_Midfielder:
					RaisePlayer(i, 10.0);
					break;
				case LT_Forward:
					RaisePlayer(i, 100.0);
					break;
				default:
					PRINT_ERROR("line type error");
					break;
				}
			}
			RaisePlayer(-i);
		}
		break;
	case LT_Midfielder:
		for (Unum i = 1; i <= TEAMSIZE; ++i){
			if (i != mpSelfState->GetUnum() && i != mpWorldState->GetTeammateGoalieUnum()){
				RaisePlayer(i);
			}
			RaisePlayer(-i, 12.0);
		}
		if (mpWorldState->GetOpponentGoalieUnum() && mpWorldState->GetOpponent(mpWorldState->GetOpponentGoalieUnum()).GetPosConf() > FLOAT_EPS){
			RaisePlayer(-mpWorldState->GetOpponentGoalieUnum(), 20.0);
		}
		break;
	case LT_Forward:
		for (Unum i = 1; i <= TEAMSIZE; ++i){
			if (i != mpSelfState->GetUnum() && i != mpWorldState->GetTeammateGoalieUnum()){
				switch (mpAgent->GetFormation().GetTeammateRoleType(i).mLineType){
				case LT_Defender:
					RaisePlayer(i, 12.0);
					break;
				case LT_Midfielder:
					RaisePlayer(i, 8.0);
					break;
				case LT_Forward:
					RaisePlayer(i, 8.0);
					break;
				default:
					PRINT_ERROR("line type error");
					break;
				}
			}
			RaisePlayer(-i, 12.0);
		}
		break;
	default:
		PRINT_ERROR("line type error");
		break;
	}
}

void VisualSystem::DoInfoGatherForGoalie()
{
	const Strategy & strategy = mpAgent->GetStrategy();

	RaiseBall();
	for (Unum i = 1; i <= TEAMSIZE; i++){
		if (i != mpSelfState->GetUnum()){
			RaisePlayer(i, 50);
		}
		RaisePlayer(-i, 50);
	}

	//�����Ӿ�����
	if(mpInfoState->GetPositionInfo().GetBallDistToTeammate(mpSelfState->GetUnum()) < 26.0){
		SetForceSeeBall();
		if (strategy.IsOppControl()){
			RaisePlayer(strategy.GetController(), 2.0);
		}
	}
}

void VisualSystem::DoVisualExecute()
{
	mBestVisualAction.mDir = GetNormalizeAngleDeg(mBestVisualAction.mDir);

	AngleDeg finalnec = GetNormalizeAngleDeg(mBestVisualAction.mDir - mPreBodyDir); //���Ҫ���ķ����뵱ǰ������˵ִ�й������ڶ�������turn���������Է���ļнǣ�������neckrelangle��

	if (fabs(finalnec) > ServerParam::instance().maxNeckAngle()) {
		if (mCanTurn) {
			if (finalnec < 0.0) {
				mpAgent->Turn(finalnec - ServerParam::instance().minNeckAngle());
				mpAgent->TurnNeck(ServerParam::instance().minNeckMoment());
			}
			else {
				mpAgent->Turn(finalnec - ServerParam::instance().maxNeckAngle());
				mpAgent->TurnNeck(ServerParam::instance().maxNeckMoment());
			}
		}
		else {
			if (finalnec < 0.0) {
				mpAgent->TurnNeck(ServerParam::instance().minNeckMoment());
			}
			else {
				mpAgent->TurnNeck(ServerParam::instance().maxNeckMoment());
			}
		}
	}
	else {
		finalnec -= mpSelfState->GetNeckDir();
		mpAgent->TurnNeck(finalnec);
	}

	if (mViewWidth != mpSelfState->GetViewWidth()) {
		mpAgent->ChangeView(mViewWidth);
	}
}

/**
* �������Ӿ�����
* @param eva �Ӿ������ǿ�ȣ������ÿeva���ڿ�һ����
*/
void VisualSystem::RaiseBall(double eva)
{
	const Strategy & strategy = mpAgent->GetStrategy();

	if (mpWorldState->GetPlayMode() < PM_Our_Mode && mpWorldState->GetPlayMode() > PM_Play_On){
		eva = Min(2.0, eva);
	}

	if (eva < FLOAT_EPS ){
		if(strategy.IsBallFree() && mpWorldState->GetPlayMode() == PM_Play_On){
			if(mpBallState->GetVelDelay() <= mpWorldState->CurrentTime() - strategy.GetLastBallFreeTime()){//��free��������������
				//���򼴽�����freeʱҪ����,�������Բ���,��������3������,�������ռ�Щ�����Ϣ
				eva = Max(Min(strategy.IsMyControl()? double(strategy.GetMyInterCycle()): strategy.GetBallFreeCycleLeft(), 3.0), 1.0);
			}
			else{ //��free��δ����������
				eva = 1.0;
				SetForceSeeBall();
			}
		}
		else if(strategy.IsTmControl()){
			double ball_dis = (mPreBallPos - mPreSelfPos).Mod();
			eva = ball_dis / 20.0 + 1.0;
			eva = Max(eva, 3.0);
		}
		else{
			double ball_dis = (mPreBallPos - mPreSelfPos).Mod();
			eva = ball_dis / 20.0;
			eva = Max(eva, 2.0);
		}
	}

	mVisualRequest.GetOfBall().mFreq = Min(mVisualRequest.GetOfBall().mFreq, eva);
}

/**
* �����Ա���Ӿ�����
* @param num ��Ա���룬+ ���ѣ�- ����
* @param eva �Ӿ������ǿ�ȣ������ÿeva���ڿ�һ�������
*/
void VisualSystem::RaisePlayer(ObjectIndex unum, double eva)
{
	if (unum == 0 || !mpWorldState->GetPlayer(unum).IsAlive() || unum == mpSelfState->GetUnum()) {
		return;
	}

	if (eva < FLOAT_EPS ){
		double dis = mpInfoState->GetPositionInfo().GetPlayerDistToPlayer(mpSelfState->GetUnum(), unum);
		if(dis < 3.0){
			eva = 6.0;
		}
		else if(dis < 6.0){
			eva = 5.0;
		}
		else if(dis < 20.0){
			eva = 10.0;
		}
		else if(dis < 40.0){
			eva = 26.0;
		}
		else{
			eva = 50.0;
		}
	}

	VisualRequest *vr = GetVisualRequest(unum);
	vr->mFreq = Min(vr->mFreq, eva);

	if (mpWorldState->GetPlayer(unum).GetPosConf() < FLOAT_EPS) {
		if(eva <= 2){
			RaiseForgotObject(unum);
		}
	}

	PlayMode pm = mpWorldState->GetPlayMode();
	if ( ((/*pm == PM_Our_Penalty_Setup ||*/ pm == PM_Our_Penalty_Ready || pm == PM_Our_Penalty_Taken) && mpWorldState->GetPlayer(unum).IsGoalie())
			|| (-unum == mpInfoState->GetPositionInfo().GetTeammateOffsideLineOpp() && mpAgent->GetFormation().GetMyRole().mLineType == LT_Forward && eva <= mpWorldState->GetPlayer(unum).GetPosDelay())) {
		RaiseForgotObject(unum);
	}
}

void VisualSystem::RaiseForgotObject(ObjectIndex unum)
{
	mIsSearching = true;

	if (unum == 0) {
		mForceToSeeObject.GetOfBall() = true;
	}
	else {
		mHighPriorityPlayerSet.insert(GetVisualRequest(unum));
	}
}

inline void VisualSystem::UpdatePredictInfo()
{
	mPreBodyDir = mpAgent->GetSelfBodyDirWithQueuedActions();
	mPreSelfPos = mpAgent->GetSelfPosWithQueuedActions();

	const Unum kickale_player = mpAgent->GetInfoState().GetPositionInfo().GetPlayerWithBall();
	if (kickale_player != 0 && kickale_player != mpSelfState->GetUnum()) {
		mPreBallPos = mpBallState->GetPos();
	}
	else {
		mPreBallPos = mpAgent->GetBallPosWithQueuedActions();
	}
}

VisualSystem::VisualAction VisualSystem::VisualRing::GetBestVisualAction(const AngleDeg left_most, const AngleDeg right_most, const AngleDeg interval_length)
{
	//���䶨��ɣ� [left, right]
	AngleDeg left = left_most;
	AngleDeg right = left;

	double sum = 0.0;

	for (; right < left + interval_length; ++right) {
		sum += Score(right);
	}
	sum += Score(right);

	double max = sum;
	AngleDeg best = (left + right) * 0.5;

	for (; right < right_most; ++right, ++left) {
		AngleDeg in = (Score(right + 1) - Score(left));

		sum += in;

		if (in < FLOAT_EPS) continue;

		if (sum > max) {
			max = sum;

			AngleDeg alpha = left + 1;
			while (Score(alpha) < FLOAT_EPS && alpha < right_most) alpha ++;
			AngleDeg beta = right + 1;
			while (Score(beta) < FLOAT_EPS && beta > alpha) beta --;
			best = (alpha + beta) * 0.5;
		}
	}

	return VisualAction(best, max);
}

bool VisualSystem::DealWithSetPlayMode()
{
	static bool check_both_side = false;
	static bool check_one_side = false;

	PlayMode play_mode = mpWorldState->GetPlayMode();
	PlayMode last_play_mode = mpWorldState->GetLastPlayMode();

	if (play_mode == PM_Our_Penalty_Ready || play_mode == PM_Our_Penalty_Setup || play_mode == PM_Our_Penalty_Taken) return false;

	if (play_mode != PM_Play_On && !(mForceToSeeObject.GetOfBall()) && !mpSelfState->IsGoalie()
			&& play_mode != PM_Before_Kick_Off
			&& last_play_mode != PM_Before_Kick_Off
			&& last_play_mode != PM_Our_Kick_Off
			&& last_play_mode != PM_Opp_Kick_Off
	){
		//����ģʽ��Ҫǿ�ƿ���ȫ���� ��ֹ��©
		if (mpWorldState->CurrentTime() < mpWorldState->GetPlayModeTime() + 3){ //ǰ�������ڵ��Ӿ���Ϣ����
			check_both_side = false;
			check_one_side = false;
		}
		else {
			if (!mpAgent->IsNewSight()) return true;

			if (!check_both_side) {
				if (!check_one_side){
					int diff = mpWorldState->CurrentTime() - mpWorldState->GetPlayModeTime();
					int flag = diff % 6;
					int base = mpSelfState->GetUnum() % 6; //��Ҫ��Ҷ�һ��ı��ӽǣ���������һ������Է������˲��

					if (flag == base) {
						mPreBodyDir = mpAgent->GetSelfBodyDirWithQueuedActions();
						mCanTurn = false;
						mBestVisualAction.mDir = GetNormalizeAngleDeg(mPreBodyDir + 90.0);
						ChangeViewWidth(VW_Wide); //new info will come in 3 cycle
						mCanForceChangeViewWidth = false;
						DoVisualExecute();
						check_one_side = true;

						return true;
					}
				}
				else {
					mBestVisualAction.mDir = GetNormalizeAngleDeg(mpSelfState->GetNeckGlobalDir() + 180.0);
					ChangeViewWidth(VW_Wide); //new info will come in 3 cycle
					mCanForceChangeViewWidth = false;
					DoVisualExecute();
					check_one_side = false;
					check_both_side = true;

					return true;
				}
			}
		}
	}

	return false;
}

void VisualSystem::DealWithSpecialObjects()
{
	static const double buffer = 0.25;
	static const int object_count = TEAMSIZE * 2 + 1;

	static const double high_priority_multi = object_count * 0.5;
	static const double force_to_see_player_multi = object_count * high_priority_multi;
	static const double force_to_see_ball_multi = object_count * force_to_see_player_multi;

	if (!mHighPriorityPlayerSet.empty()) {
		for (std::set<VisualRequest*>::iterator it = mHighPriorityPlayerSet.begin(); it != mHighPriorityPlayerSet.end(); ++it) {
			if (!(*it)->mValid) continue;
			(*it)->mScore = (*it)->Multi() * (mForceToSeeObject[(*it)->mUnum]? force_to_see_player_multi: high_priority_multi);

			if ((*it)->mpObject->GetPosConf() > 0.9 && (*it)->mPreDistance < ServerParam::instance().visibleDistance() - buffer) {
				SetCritical(true);
			}
		}
	}

	if (mForceToSeeObject[0] && mVisualRequest[0].mValid) {
		if (mpBallState->GetPosConf() > 0.9 && mVisualRequest[0].mPreDistance < ServerParam::instance().visibleDistance() - buffer) {
			if (mpBallState->GetPosDelay() == 0) {
				mVisualRequest[0].mScore = FLOAT_EPS; //just sense it
			}

			SetCritical(true);
		}
		else {
			mVisualRequest[0].mScore = mVisualRequest[0].Multi() * force_to_see_ball_multi;
		}
	}
}

void VisualSystem::SetVisualRing()
{
	ObjectArray<double> object_poss_sum;
	object_poss_sum.fill(0.0);

	mVisualRing.Clear();

	for (int i = -TEAMSIZE; i <= TEAMSIZE; ++i) {
		const VisualRequest & visual_request = mVisualRequest[i];

		if (!visual_request.mValid) continue;

		double & sum_poss = object_poss_sum[i];
		const std::list<BeliefState::Grid*> & set = mpAgent->GetBeliefState()->GetAppearanceSet(i);

		for (std::list<BeliefState::Grid*>::const_iterator it = set.begin(); it != set.end(); ++it) {
			const double poss = (*it)->GetAppearProb(i);
			Assert(poss > 0.0);
			sum_poss += poss;
			mVisualRing.Score(((*it)->mPos - mPreSelfPos).Dir() - mPreBodyDir) += poss * visual_request.mScore;
		}
	}

	for (int i = -TEAMSIZE; i <= TEAMSIZE; ++i) {
		if (i == 0) {
			if (object_poss_sum[0] < FLOAT_EPS && mVisualRequest[0].mValid) {
				AngleDeg dir = (mPreBallPos - mPreSelfPos).Dir() - mPreBodyDir;
				for (int i = -10; i <= 10; ++i) {
					mVisualRing.Score(dir + i) += mVisualRequest[0].mScore * 0.05;
				}
			}
		}
		else {
			if (object_poss_sum[i] < FLOAT_EPS && mVisualRequest[i].mValid) {
				AngleDeg dir = (mpWorldState->GetPlayer(i).GetPredictedPos() - mPreSelfPos).Dir() - mPreBodyDir;
				for (int j = -5; j <= 5; ++j) {
					mVisualRing.Score(dir + j) += mVisualRequest[i].mScore * 0.1;
				}
			}
		}
	}
}

void VisualSystem::GetBestVisualAction()
{
	if (mIsCritical) {
		if (NewSightComeCycle(VW_Wide) == 1) {
			ChangeViewWidth(VW_Wide);
		}
		else if (NewSightComeCycle(VW_Normal) == 1) {
			ChangeViewWidth(VW_Normal);
		}
		else {
			ChangeViewWidth(VW_Narrow);
		}

		mBestVisualAction = GetBestVisualActionWithViewWidth(mViewWidth, true);
	}
	else {
		if (mViewWidth != VW_Narrow && !mIsSearching) {
			mBestVisualAction = GetBestVisualActionWithViewWidth(mViewWidth, true);
		}
		else {
			mCanForceChangeViewWidth = true;


			Array<VisualAction, 3> best_visual_action;

			//����3.0�����ڣ�������֤ÿ���ڶ��ܸ�֪���򣬳��Ƕ����������״̬Ԥ����а���
			const bool force =
					mpWorldState->GetPlayMode() != PM_Our_Penalty_Taken
					&& mpWorldState->GetBall().GetPosDelay() == 0 && mpWorldState->GetHistory(1)->GetBall().GetPosDelay() == 0
					&& mpAgent->GetInfoState().GetPositionInfo().GetClosestOpponentDistToBall() > 3.0;

			if (force && PlayerParam::instance().SaveTextLog()) {
				Logger::instance().GetTextLogger("sure_ball") << mpWorldState->CurrentTime() << std::endl;
			}

			if (NewSightComeCycle(VW_Wide) == 1) {
				best_visual_action[2] = GetBestVisualActionWithViewWidth(VW_Wide, force);
			}
			else if (NewSightComeCycle(VW_Normal) == 1) {
				best_visual_action[1] = GetBestVisualActionWithViewWidth(VW_Normal, force);
				best_visual_action[2] = GetBestVisualActionWithViewWidth(VW_Wide, force);
			}
			else {
				best_visual_action[0] = GetBestVisualActionWithViewWidth(VW_Narrow, force);
				best_visual_action[1] = GetBestVisualActionWithViewWidth(VW_Normal, force);
				best_visual_action[2] = GetBestVisualActionWithViewWidth(VW_Wide, force);
			}

			const double buffer = FLOAT_EPS;
			if (best_visual_action[0].mScore > best_visual_action[1].mScore - buffer) {
				if (best_visual_action[0].mScore > best_visual_action[2].mScore - buffer) {
					//narrow
					ChangeViewWidth(VW_Narrow);
					mBestVisualAction = best_visual_action[0];
				}
				else {
					//wide
					ChangeViewWidth(VW_Wide);
					mBestVisualAction = best_visual_action[2];
				}
			}
			else {
				if (best_visual_action[1].mScore > best_visual_action[2].mScore - buffer) {
					//normal
					ChangeViewWidth(VW_Normal);
					mBestVisualAction = best_visual_action[1];
				}
				else {
					//wide
					ChangeViewWidth(VW_Wide);
					mBestVisualAction = best_visual_action[2];
				}
			}
		}
	}

	mBestVisualAction.mDir += mPreBodyDir;
	//Assert(mBestVisualAction.mScore > 0.0);
}

VisualSystem::VisualAction VisualSystem::GetBestVisualActionWithViewWidth(ViewWidth view_width, bool force)
{
	if (force || GetSenseBallCycle() >= NewSightComeCycle(view_width)) {
		AngleDeg max_turn_ang = mCanTurn? mpSelfState->GetMaxTurnAngle(): 0.0;
		AngleDeg half_view_angle = sight::ViewAngle(view_width) * 0.5;
		AngleDeg neck_left_most = ServerParam::instance().minNeckAngle() - max_turn_ang; //���ӿ��Ե���ļ��޽Ƕȣ�����ڵ�ǰ������ǰ�����ԣ�
		AngleDeg neck_right_most = ServerParam::instance().maxNeckAngle() + max_turn_ang;
		AngleDeg left_most = neck_left_most - half_view_angle;
		AngleDeg right_most = neck_right_most + half_view_angle;

		VisualAction best_visual_action = mVisualRing.GetBestVisualAction(left_most, right_most, half_view_angle * 2.0);

		best_visual_action.mScore /= NewSightWaitCycle(view_width);

		Assert(!IsInvalid(best_visual_action.mScore));

		return best_visual_action;
	}
	else {
		return VisualAction();
	}
}

bool VisualSystem::ForceSearchBall()
{
	if (!mpSelfState->IsIdling() && !mVisualRequest[0].mValid) { //force to scan
		mpAgent->GetActionEffector().ResetForScan();

		if (NewSightComeCycle(VW_Wide) == 1) {
			mpAgent->ChangeView(VW_Wide);
		}
		else if (NewSightComeCycle(VW_Normal) == 1){
			mpAgent->ChangeView(VW_Normal);
		}
		else {
			mpAgent->ChangeView(VW_Narrow);
		}

		mpAgent->Turn(sight::ViewAngle(VW_Narrow) - 5.0);

		return true;
	}
	return false;
}

void VisualSystem::DoDecision()
{
	if (!mpAgent->IsNewSight()){
		if (mCanForceChangeViewWidth) {
			mViewWidth = VW_Narrow; //����Ϊխ�ӽ�
		}
		else {
			mViewWidth = mpSelfState->GetViewWidth();
		}
	}

	DoInfoGather(); //������
	EvaluateVisualRequest(); //����

	if (!ForceSearchBall()) {
		DealVisualRequest(); //��������
		DoVisualExecute(); //�Ӿ�ִ��
	}
}

int VisualSystem::GetSenseBallCycle()
{
	if (mSenseBallCycle == 0) { //������ģ������ʲôʱ����Ը�֪����
		if (mVisualRequest[0].mValid) {
			if (mVisualRequest[0].mPreDistance < ServerParam::instance().visibleDistance()) {
				mSenseBallCycle = 1;
			}
			else {
				PlayerInterceptInfo int_info;
				VirtualSelf virtual_self(*mpSelfState);

				virtual_self.UpdateIsGoalie(false); //������ÿ��˷�Χ�����֪��Χ�������

				int_info.mRes = IR_None;
				int_info.mpPlayer = & virtual_self;

				InterceptInfo::CalcTightInterception(*mpBallState, & int_info);

				mSenseBallCycle = int_info.mMinCycle;
				mSenseBallCycle = Max(mSenseBallCycle, 1);
			}
		}
		else {
			mSenseBallCycle = 1000;
		}
	}

	return mSenseBallCycle;
}

void VisualSystem::SetForceSeeBall()
{
	mForceToSeeObject.GetOfBall() = true;

	Logger::instance().GetTextLogger("force_to_see") << mpWorldState->CurrentTime() << ": ball" << std::endl;
}

void VisualSystem::SetForceSeePlayer(Unum i)
{
	if (i != 0) {
		mForceToSeeObject[i] = true;
		mHighPriorityPlayerSet.insert(GetVisualRequest(i));

		Logger::instance().GetTextLogger("force_to_see") << mpWorldState->CurrentTime() << ": player " << i << std::endl;
	}
}