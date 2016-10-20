//
// NavigationType.cpp
//

#include "NavigationType.h"
#include "MQ2Navigation.h"

//----------------------------------------------------------------------------

MQ2NavigationType::MQ2NavigationType(MQ2NavigationPlugin* nav_)
	: MQ2Type("Navigation")
	, m_nav(nav_)
{
	TypeMember(Active);
	TypeMember(Paused);
	TypeMember(MeshLoaded);
	TypeMember(PathExists);
	TypeMember(PathLength);
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
