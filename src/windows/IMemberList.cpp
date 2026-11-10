#include "IMemberList.hpp"
#include "MemberList.hpp"

IMemberList* IMemberList::CreateMemberList(HWND hwnd, LPRECT lprect)
{
	return MemberList::Create(hwnd, lprect);
}

void IMemberList::InitializeClasses()
{
	MemberList::InitializeClass();
}
