//
// NavigationType.cpp
//

#include "pch.h"
#include "NavigationType.h"

#include "plugin/MQ2Navigation.h"

//----------------------------------------------------------------------------

std::unique_ptr<MQ2NavigationType> g_mq2NavigationType;
std::unique_ptr<MQ2NavPathType> g_mq2NavPathType;

MQ2NavigationType::MQ2NavigationType()
	: MQ2Type("Navigation")
	, m_nav(g_mq2Nav.get())
{
	TypeMember(Active);
	TypeMember(Paused);
	TypeMember(MeshLoaded);
	TypeMember(PathExists);
	TypeMember(PathLength);

	//TypeMember(CurrentPath);
}

MQ2NavigationType::~MQ2NavigationType()
{
}

bool MQ2NavigationType::GetMember(MQ2VARPTR VarPtr, PCHAR Member, PCHAR Index, MQ2TYPEVAR &Dest)
{
	PMQ2TYPEMEMBER pMember = MQ2NavigationType::FindMember(Member);
	if (pMember) switch ((NavigationMembers)pMember->ID) {
	case Active:
		Dest.Type = pBoolType;
		Dest.DWord = m_nav->IsActive();
		return true;
	case Paused:
		Dest.Type = pBoolType;
		Dest.DWord = m_nav->IsPaused();
		return true;
	case MeshLoaded:
		Dest.Type = pBoolType;
		Dest.DWord = m_nav->IsMeshLoaded();
		return true;
	case PathExists:
		Dest.Type = pBoolType;
		Dest.DWord = m_nav->CanNavigateToPoint(Index);
		return true;
	case PathLength:
		Dest.Type = pFloatType;
		Dest.Float = m_nav->GetNavigationPathLength(Index);
		return true;


	case CurrentPath:
		//Dest.Type = g_mq2NavPathType.get();
		//Dest.
		break;
	}

	strcpy_s(DataTypeTemp, "NULL");
	Dest.Type = pStringType;
	Dest.Ptr = &DataTypeTemp[0];
	return true;
}

bool MQ2NavigationType::ToString(MQ2VARPTR VarPtr, PCHAR Destination)
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

bool MQ2NavPathType::GetMember(MQ2VARPTR VarPtr, PCHAR Member, PCHAR Index, MQ2TYPEVAR& Dest)
{
	return false;
}

bool MQ2NavPathType::ToString(MQ2VARPTR VarPtr, PCHAR Destination)
{
	return false;
}

//----------------------------------------------------------------------------

static BOOL NavigateData(PCHAR szName, MQ2TYPEVAR& Dest)
{
	Dest.DWord = 1;
	Dest.Type = g_mq2NavigationType.get();
	return true;
}

void InitializeMQ2NavMacroData()
{
	g_mq2NavigationType = std::make_unique<MQ2NavigationType>();
	AddMQ2Data("Navigation", NavigateData);
	AddMQ2Data("Nav", NavigateData);

	g_mq2NavPathType = std::make_unique<MQ2NavPathType>();
}

void ShutdownMQ2NavMacroData()
{
	RemoveMQ2Data("Navigation");
	RemoveMQ2Data("Nav");

	g_mq2NavigationType.reset();
	g_mq2NavPathType.reset();
}
