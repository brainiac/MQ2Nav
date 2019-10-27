//
// NavigationType.h
//

// This class defines the custom MQ2 data type for use in macros

#pragma once

#include <mq/Plugin.h>

#include <memory>

//----------------------------------------------------------------------------

void InitializeMQ2NavMacroData();
void ShutdownMQ2NavMacroData();

//----------------------------------------------------------------------------

class MQ2NavigationPlugin;
class NavigationPath;

class MQ2NavigationType : public MQ2Type
{
public:
	enum NavigationMembers {
		Active = 1,
		Paused = 2,
		MeshLoaded = 3,
		PathExists = 4,
		PathLength = 5,

		// These return MQ2NavPathType
		CurrentPath = 6,
		PathTo = 7,

		Setting = 8,
		Velocity = 9,
	};

	MQ2NavigationType();
	virtual ~MQ2NavigationType();

	virtual bool GetMember(MQVarPtr VarPtr, PCHAR Member, PCHAR Index, MQTypeVar& Dest) override;
	virtual bool ToString(MQVarPtr VarPtr, PCHAR Destination) override;

	virtual bool FromData(MQVarPtr& VarPtr, MQTypeVar& Source) override { return false; }
	virtual bool FromString(MQVarPtr& VarPtr, PCHAR Source) override { return false; }

private:
	MQ2NavigationPlugin* m_nav;
};

extern MQ2NavigationType* g_mq2NavigationType;

//----------------------------------------------------------------------------

class MQ2NavPathType : public MQ2Type
{
public:
	enum NavPathMembers {
		IsValid = 1,
	};

	MQ2NavPathType();
	virtual ~MQ2NavPathType();

	virtual bool GetMember(MQVarPtr VarPtr, PCHAR Member, PCHAR Index, MQTypeVar& Dest) override;
	virtual bool ToString(MQVarPtr VarPtr, PCHAR Destination) override;

	virtual bool FromData(MQVarPtr& VarPtr, MQTypeVar& Source) override { return false; }
	virtual bool FromString(MQVarPtr& VarPtr, PCHAR Source) override { return false; }

protected:
};

extern MQ2NavPathType* g_mq2NavPathType;
