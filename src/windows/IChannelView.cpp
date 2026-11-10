#include "IChannelView.hpp"
#include "ChannelView.hpp"

IChannelView* IChannelView::CreateChannelView(HWND hwnd, LPRECT rect)
{
	return ChannelView::Create(hwnd, rect);
}

void IChannelView::InitializeClasses()
{
	ChannelView::InitializeClass();
}
