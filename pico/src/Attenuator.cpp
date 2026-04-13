#include "Attenuator.h"
#include "att_types.h"
#include "att_26ghz.h"
#include "att_135db.h"
#include "att_a.h"
#include "att_b.h"

/* Static instances – one per concrete type.  Only the active one is ever
 * used at runtime; all four exist to avoid heap allocation on the Pico. */
static Att26GHz  s_att_26ghz;
static Att135dB  s_att_135db;
static AttA      s_att_a;
static AttB      s_att_b;

/* This is the ONE switch/case over the attenuator type in the whole project. */
Attenuator* create_attenuator(int type)
{
    switch(type) {
        case ATTENUATOR_26_5GHz:  return &s_att_26ghz;
        case ATTENUATOR_RS_135DB: return &s_att_135db;
        case ATTENUATOR_A:        return &s_att_a;
        case ATTENUATOR_B:        return &s_att_b;
        default:                  return nullptr;
    }
}
