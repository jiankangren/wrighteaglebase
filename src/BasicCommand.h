#ifndef __BasicCommand_H__
#define __BasicCommand_H__


#include <list>
#include <cstring>
#include <string>
#include "Types.h"
#include "Utilities.h"
#include "Geometry.h"

enum EarMode
{
	EM_Null,
	EM_Partial,
	EM_Complete,
    EM_All
};

enum CommandType
{
	CT_None,
	CT_Turn,
	CT_Dash,
	CT_TurnNeck,
	CT_Say,
	CT_Attentionto,
	CT_Kick,
	CT_Tackle,
	CT_Pointto,
	CT_Catch,
	CT_Move,
	CT_ChangeView,
	CT_Compression,
	CT_SenseBody,
	CT_Score,
	CT_Bye,
	CT_Done,
	CT_Clang,
	CT_Ear,
	CT_SynchSee,
	CT_ChangePlayerType
};

class Agent;

struct CommandInfo
{
	CommandType mType;
	int mLevel;
	int mMinVer;
	int mMaxVer;
	bool mMutex;
	Time mTime;
	double mDist;
	double mPower;
	double mAngle;
	Vector mMovePos;
	ViewWidth mViewWidth;
    std::string mString;

	CommandInfo()
	{
		mType = CT_None;
	}

	const double & GetPower() const { return mPower; }
	const double & GetAngle() const { return mAngle; }
	const Vector & GetMovePos() const { return mMovePos; }
};


class BasicCommand
{
public:
	BasicCommand(const Agent & agent): mAgent(agent) {}
	virtual ~BasicCommand() {}

	bool Execute(std::list<CommandInfo> &command_queue);

	const double & GetPower() const { return mCommandInfo.GetPower(); }
	const double & GetAngle() const { return mCommandInfo.GetAngle(); }
	const Vector & GetMovePos() const { return mCommandInfo.GetMovePos(); }

protected:
	const Agent     & mAgent;
	CommandInfo mCommandInfo; // ��Plan()�и�ֵ����Execute()��ѹ�����
};


/**
* ������server���Խ��յ�ԭ��������࣬�����˳����SoccerServer - Player::parseCommand()�е�˳��һ��
*/

class Turn : public BasicCommand
{
public:
	Turn(const Agent & agent);
	~Turn() {}

	void Plan(double moment);
};


class Dash : public BasicCommand
{
public:
	Dash(const Agent & agent);
	~Dash() {}

	void Plan(double power, AngleDeg dir);
};


class TurnNeck : public BasicCommand
{
public:
	TurnNeck(const Agent & agent);
	~TurnNeck() {}

	void Plan(double moment);
};


class Say : public BasicCommand
{
public:
	Say(const Agent & agent);
	~Say() {}

	void Plan(std::string msg);
};


class Attentionto : public BasicCommand
{
public:
	Attentionto(const Agent & agent);
	~Attentionto() {}

	void Plan(bool on, Unum num = 0);
};


class Kick : public BasicCommand
{
public:
	Kick(const Agent & agent);
	~Kick() {}

	void Plan(double power, double dir);
};


class Tackle : public BasicCommand
{
public:
	Tackle(const Agent & agent);
	~Tackle() {}

	void Plan(double dir, const bool foul);
};


class Pointto : public BasicCommand
{
public:
	Pointto(const Agent & agent);
	~Pointto() {}

	void Plan(bool on, double dist = 0.0, double dir = 0.0);
};


class Catch : public BasicCommand
{
public:
	Catch(const Agent & agent);
	~Catch() {}

	void Plan(double dir);
};


class Move : public BasicCommand
{
public:
	Move(const Agent & agent);
	~Move() {}

	void Plan(Vector pos);
};

class ChangeView : public BasicCommand
{
public:
	ChangeView(const Agent & agent);
	~ChangeView() {}

	void Plan(ViewWidth view_width);
};


class Compression : public BasicCommand
{
public:
	Compression(const Agent & agent);
	~Compression() {}

	void Plan(int level);
};


class SenseBody : public BasicCommand
{
public:
	SenseBody(const Agent & agent);
	~SenseBody() {}

	void Plan();
};


class Score : public BasicCommand
{
public:
	Score(const Agent & agent);
	~Score() {}

	void Plan();
};


class Bye : public BasicCommand
{
public:
	Bye(const Agent & agent);
	~Bye() {}

	void Plan();
};


class Done : public BasicCommand
{
public:
	Done(const Agent & agent);
	~Done() {}

	void Plan();
};


class Clang : public BasicCommand
{
public:
	Clang(const Agent & agent);
	~Clang() {}

	void Plan(int min_ver, int max_ver);
};


class Ear : public BasicCommand
{
public:
	Ear(const Agent & agent);
	~Ear() {}

	void Plan(bool on, bool our_side, EarMode ear_mode = EM_All);
};


class SynchSee : public BasicCommand
{
public:
	SynchSee(const Agent & agent);
	~SynchSee() {}

	void Plan();
};

class ChangePlayerType : public BasicCommand
{
public:
	ChangePlayerType(const Agent & agent);
	~ChangePlayerType() {}

	void Plan(Unum num, int player_type);
};

#endif
