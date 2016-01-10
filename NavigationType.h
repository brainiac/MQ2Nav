//
// NavigationType.h
//

// This class defines the custom MQ2 data type for use in macros

#pragma once

#include "MQ2Plugin.h"

//----------------------------------------------------------------------------

class MQ2NavigationPlugin;

class MQ2NavigationType : public MQ2Type
{
public:
	enum NavigationMembers {
		Active = 1,
		Paused = 2,
		MeshLoaded = 3,
		PathExists = 4,
		PathLength = 5,
	};

	MQ2NavigationType(MQ2NavigationPlugin* nav_);
	virtual ~MQ2NavigationType();

	virtual bool GetMember(MQ2VARPTR VarPtr, PCHAR Member, PCHAR Index, MQ2TYPEVAR& Dest) override;
	virtual bool ToString(MQ2VARPTR VarPtr, PCHAR Destination) override;

	virtual bool FromData(MQ2VARPTR& VarPtr, MQ2TYPEVAR& Source) override { return false; }
	virtual bool FromString(MQ2VARPTR& VarPtr, PCHAR Source) override { return false; }

private:
	MQ2NavigationPlugin* m_nav;
};
