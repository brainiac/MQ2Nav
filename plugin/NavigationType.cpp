//
// NavigationType.cpp
//

#include "pch.h"
#include "NavigationType.h"
#include "NavigationPath.h"

#include "plugin/MQ2Navigation.h"
#include "plugin/PluginSettings.h"

//----------------------------------------------------------------------------

MQ2NavigationType* g_mq2NavigationType = nullptr;
MQ2NavPathType* g_mq2NavPathType = nullptr;

MQ2NavigationType::MQ2NavigationType()
	: MQ2Type("Navigation")
	, m_nav(g_mq2Nav)
{
	TypeMember(Active);
	TypeMember(Paused);
	TypeMember(MeshLoaded);
	TypeMember(PathExists);
	TypeMember(PathLength);
	TypeMember(Setting);
	TypeMember(Velocity);
	TypeMember(CurrentPathDistance);
}

MQ2NavigationType::~MQ2NavigationType()
{
}

bool MQ2NavigationType::GetMember(MQVarPtr VarPtr, const char* Member, PCHAR Index, MQTypeVar& Dest)
{
	MQTypeMember* pMember = MQ2NavigationType::FindMember(Member);
	if (pMember) switch ((NavigationMembers)pMember->ID)
	{
	case Active:
		Dest.Type = mq::datatypes::pBoolType;
		Dest.DWord = m_nav->IsActive();
		return true;
	case Paused:
		Dest.Type = mq::datatypes::pBoolType;
		Dest.DWord = m_nav->IsPaused();
		return true;
	case MeshLoaded:
		Dest.Type = mq::datatypes::pBoolType;
		Dest.DWord = m_nav->IsMeshLoaded();
		return true;
	case PathExists:
		Dest.Type = mq::datatypes::pBoolType;
		Dest.DWord = m_nav->CanNavigateToPoint(Index);
		return true;
	case PathLength:
		Dest.Type = mq::datatypes::pFloatType;
		Dest.Float = m_nav->GetNavigationPathLength(Index);
		return true;
	case CurrentPath:
		//Dest.Type = g_mq2NavPathType.get();
		//Dest.
		break;
	case CurrentPathDistance:
	{
		auto m_activePath = m_nav->GetActivePath();
		if (m_activePath)
		{
			Dest.Type = mq::datatypes::pFloatType;
			Dest.Float = m_activePath->GetPathTraversalDistance();
			return true;
		}
		return false;
	}
	case Setting:
		Dest.Type = mq::datatypes::pStringType;
		if (nav::ReadIniSetting(Index, &DataTypeTemp[0], DataTypeTemp.size()))
		{
			Dest.Ptr = &DataTypeTemp[0];
			return true;
		}
		return false;

	case Velocity: {
		Dest.Type = mq::datatypes::pIntType;
		Dest.Int = static_cast<int>(glm::round(GetMyVelocity()));
		return true;
	}
	}

	strcpy_s(DataTypeTemp, "NULL");
	Dest.Type = mq::datatypes::pStringType;
	Dest.Ptr = &DataTypeTemp[0];
	return true;
}

bool MQ2NavigationType::ToString(MQVarPtr VarPtr, PCHAR Destination)
{
	strcpy_s(Destination, MAX_STRING, "TRUE");
	return true;
}

//----------------------------------------------------------------------------

MQ2NavPathType::MQ2NavPathType()
	: MQ2Type("NavPath")
{
}

MQ2NavPathType::~MQ2NavPathType()
{
}

bool MQ2NavPathType::GetMember(MQVarPtr VarPtr, const char* Member, PCHAR Index, MQTypeVar& Dest)
{
	return false;
}

bool MQ2NavPathType::ToString(MQVarPtr VarPtr, PCHAR Destination)
{
	return false;
}

//----------------------------------------------------------------------------

static bool NavigateData(const char* szName, MQTypeVar& Dest)
{
	Dest.DWord = 1;
	Dest.Type = g_mq2NavigationType;
	return true;
}

void InitializeMQ2NavMacroData()
{
	g_mq2NavigationType = new MQ2NavigationType();
	AddMQ2Data("Navigation", NavigateData);
	AddMQ2Data("Nav", NavigateData);

	g_mq2NavPathType = new MQ2NavPathType();
}

void ShutdownMQ2NavMacroData()
{
	RemoveMQ2Data("Navigation");
	RemoveMQ2Data("Nav");

	delete g_mq2NavigationType;
	g_mq2NavigationType = nullptr;

	delete g_mq2NavPathType;
	g_mq2NavPathType = nullptr;
}
