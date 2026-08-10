#include "cri_xpt.h"

extern void SVM_CallErr1(Char8 *msg);

void SJERR_CallErr(Char8 *msg)
{
	SVM_CallErr1(msg);
}
