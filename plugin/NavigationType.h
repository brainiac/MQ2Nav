//
// NavigationType.h
//

// This class defines the custom MQ2 data type for use in macros

#pragma once

#include <MQ2Plugin.h>

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
		Velocity = 9,
	};

	MQ2NavigationType();
	virtual ~MQ2NavigationType();

	virtual bool GetMember(MQ2VARPTR VarPtr, PCHAR Member, PCHAR Index, MQ2TYPEVAR& Dest) override;
	virtual bool ToString(MQ2VARPTR VarPtr, PCHAR Destination) override;

	virtual bool FromData(MQ2VARPTR& VarPtr, MQ2TYPEVAR& Source) override { return false; }
	virtual bool FromString(MQ2VARPTR& VarPtr, PCHAR Source) override { return false; }

private:
	MQ2NavigationPlugin* m_nav;
};

extern std::unique_ptr<MQ2NavigationType> g_mq2NavigationType;

//----------------------------------------------------------------------------

class MQ2NavPathType : public MQ2Type
{
public:
	enum NavPathMembers {
		IsValid = 1,
		
	};

	MQ2NavPathType();
	virtual ~MQ2NavPathType();

	virtual bool GetMember(MQ2VARPTR VarPtr, PCHAR Member, PCHAR Index, MQ2TYPEVAR& Dest) override;
	virtual bool ToString(MQ2VARPTR VarPtr, PCHAR Destination) override;

	virtual bool FromData(MQ2VARPTR& VarPtr, MQ2TYPEVAR& Source) override { return false; }
	virtual bool FromString(MQ2VARPTR& VarPtr, PCHAR Source) override { return false; }

protected:
};

extern std::unique_ptr<MQ2NavPathType> g_mq2NavPathType;
