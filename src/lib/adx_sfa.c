#include "cri_xpt.h"

static Sint32 sfa_init_cnt;
static Sint32 sfa_work;

void SFA_Finish(void)
{
	sfa_init_cnt--;
}

void SFA_Init(void)
{
	sfa_init_cnt++;
}
