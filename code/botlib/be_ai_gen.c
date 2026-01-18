

/*****************************************************************************
 * name:		be_ai_gen.c
 *
 * desc:		genetic selection
 *
 * $Archive: /MissionPack/code/botlib/be_ai_gen.c $
 *
 *****************************************************************************/

#include "../qcommon/q_shared.h"
#include "l_memory.h"
#include "l_log.h"
#include "l_utils.h"
#include "l_script.h"
#include "l_precomp.h"
#include "l_struct.h"
#include "aasfile.h"
#include "botlib.h"
#include "be_aas.h"
#include "be_aas_funcs.h"
#include "be_interface.h"
#include "be_ai_gen.h"

//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
#include "../qcommon/q_shared.h"
#include "l_memory.h"
#include "be_aas.h"
#include "be_ai_gen.h"

//===========================================================================
// GeneticSelection
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int GeneticSelection(int numranks, float* rankings)
{
	float sum, r;
	int i, index;

	sum = 0;
	for (i = 0; i < numranks; i++)
	{
		if (rankings[i] < 0) continue;
		sum += rankings[i];
	}

	if (sum > 0)
	{
		// Метод "Рулетки" (Roulette Wheel Selection):
		// выбираем случайную точку на шкале общей суммы рейтингов.
		r = random() * sum;
		for (i = 0; i < numranks; i++)
		{
			if (rankings[i] < 0) continue;
			r -= rankings[i];
			if (r <= 0) return i;
		}
	}

	// Если сумма 0 или все рейтинги отрицательные, выбираем случайно.
	// Исправлено C4244: Явное приведение к int
	index = (int)(random() * (float)numranks);

	for (i = 0; i < numranks; i++)
	{
		if (rankings[index] >= 0) return index;
		index = (index + 1) % numranks;
	}
	return 0;
}
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int GeneticParentsAndChildSelection(int numranks, float *ranks, int *parent1, int *parent2, int *child)
{
	float rankings[256], max;
	int i;

	if (numranks > 256)
	{
		botimport.Print(PRT_WARNING, "GeneticParentsAndChildSelection: too many bots\n");
		*parent1 = *parent2 = *child = 0;
		return qfalse;
	} //end if
	for (max = 0, i = 0; i < numranks; i++)
	{
		if (ranks[i] < 0) continue;
		max++;
	} //end for
	if (max < 3)
	{
		botimport.Print(PRT_WARNING, "GeneticParentsAndChildSelection: too few valid bots\n");
		*parent1 = *parent2 = *child = 0;
		return qfalse;
	} //end if
	Com_Memcpy(rankings, ranks, sizeof(float) * numranks);
	//select first parent
	*parent1 = GeneticSelection(numranks, rankings);
	rankings[*parent1] = -1;
	//select second parent
	*parent2 = GeneticSelection(numranks, rankings);
	rankings[*parent2] = -1;
	//reverse the rankings
	max = 0;
	for (i = 0; i < numranks; i++)
	{
		if (rankings[i] < 0) continue;
		if (rankings[i] > max) max = rankings[i];
	} //end for
	for (i = 0; i < numranks; i++)
	{
		if (rankings[i] < 0) continue;
		rankings[i] = max - rankings[i];
	} //end for
	//select child
	*child = GeneticSelection(numranks, rankings);
	return qtrue;
} //end of the function GeneticParentsAndChildSelection
