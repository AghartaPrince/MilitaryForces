/*
 * $Id: g_mfq3util.c,v 1.10 2002-01-26 23:48:32 thebjoern Exp $
*/



#include "g_local.h"




int canLandOnIt( gentity_t *ent )
{
	if( strcmp( ent->classname, "func_runway" ) == 0 ||
		strcmp( ent->classname, "func_plat" ) == 0 ||
		strcmp( ent->classname, "func_train" ) == 0 ||
		strcmp( ent->classname, "func_door" ) == 0 ) {
		return 1;
	}
	return 0;
}


void unlock( gentity_t* ent )
{
	ent->locktime = 0;
	ent->tracktarget = 0;
	ent->s.tracktarget = ENTITYNUM_NONE;
	ent->client->ps.stats[STAT_LOCKINFO] &= ~(LI_TRACKING|LI_LOCKING);
	ent->client->ps.tracktarget = ENTITYNUM_NONE;
}

static void track( gentity_t* ent, gentity_t* target )
{
	ent->tracktarget = target;
	ent->locktime = level.time;
	ent->s.tracktarget = ent->tracktarget->s.number;
	ent->client->ps.stats[STAT_LOCKINFO] |= LI_TRACKING;
	ent->client->ps.tracktarget = ent->tracktarget->s.number;
}

void updateTargetTracking( gentity_t *ent )
{
	unsigned long	targetcat = 0;
	trace_t			tr;
	vec3_t			forward, endpos, dir;
	float			radarrange;
	qboolean		buildings = qfalse;
	gentity_t		*test;
	float			cone;

	// what can we lock on ?
	if(	ent->client->ps.ONOFF & OO_RADAR_AIR ) {
		targetcat = CAT_PLANE|CAT_HELO;
		radarrange = availableVehicles[ent->client->vehicle].radarRange;
		cone = availableVehicles[ent->client->vehicle].trackCone;
	} else if( ent->client->ps.ONOFF & OO_RADAR_GROUND ) {
		targetcat = CAT_GROUND;
		buildings = qtrue;
		radarrange = availableVehicles[ent->client->vehicle].radarRange2;
		cone = availableVehicles[ent->client->vehicle].trackCone2;
	}
	
	if( !targetcat ) {
		if( ent->client->ps.stats[STAT_LOCKINFO] ) unlock(ent);
		return;
	}

	// weapon range and vehicle direction
	if( availableVehicles[ent->client->vehicle].id&CAT_ANY & CAT_GROUND ) {
		vec3_t	right, up, temp;
		AngleVectors( ent->client->ps.vehicleAngles, forward, right, up );
		RotatePointAroundVector( temp, up, forward, ((float)ent->client->ps.turretAngle)/10 );
		CrossProduct( up, temp, right );
		RotatePointAroundVector( dir, right, temp, ((float)ent->client->ps.gunAngle)/10 );
		VectorCopy( dir, forward );
	} else {
		VectorCopy( ent->s.angles, dir );
		AngleVectors( dir, forward, 0, 0 );
	}


	// no target yet
	if( !ent->tracktarget ) {
		VectorMA( ent->r.currentOrigin, radarrange, forward, endpos );
		trap_Trace( &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, endpos, ent->s.number, MASK_ALL );
		// nothing found
		if( tr.entityNum == ENTITYNUM_NONE ) {
			return;
		}
		// found something
		test = &g_entities[tr.entityNum];
		if( test->s.eType == ET_VEHICLE && test->client && 
			(availableVehicles[test->client->vehicle].id&CAT_ANY & targetcat) ) {
	//		G_Printf("found target: player %s\n", test->client->pers.netname );
			track(ent, test);
		} else if( test->s.eType == ET_MISC_VEHICLE &&
			(availableVehicles[test->s.modelindex].id&CAT_ANY & targetcat) ) {
	//		G_Printf("found target: drone %s\n", test->targetname );
			track(ent, test);
		} else {
			if( buildings ) {
				if( test->s.eType == ET_EXPLOSIVE && test->takedamage ) {
					track(ent, test);
				}
			} else {
				return;
			}
		}
	} else { // update existing target
		vec3_t diff;
		float dot, dist;
		if( ent->tracktarget->s.eType == ET_EXPLOSIVE ) {
			vec3_t	mid;
			VectorAdd( ent->tracktarget->r.absmax, ent->tracktarget->r.absmin, mid );
			VectorScale( mid, 0.5f, mid );
			VectorSubtract( mid, ent->r.currentOrigin, diff );
		} else {
			VectorSubtract( ent->tracktarget->r.currentOrigin, ent->r.currentOrigin, diff );
		}
		// check within range
		dist = VectorNormalize( diff );
		if( dist > radarrange ) {
			unlock(ent);
			return;
		}
		// check within cone
		dot = DotProduct( forward, diff );
		if( dot < cone ) {
			unlock(ent);
			return;
		} else if( dot < availableWeapons[ent->s.weaponIndex].lockcone ) { // roughly 10 degrees
			ent->locktime = level.time;
			ent->client->ps.stats[STAT_LOCKINFO] &= ~LI_LOCKING;
		}
			
		// check LOS
		VectorMA( ent->r.currentOrigin, dist, diff, endpos );
		trap_Trace( &tr, ent->r.currentOrigin, 0, 0, endpos, ent->s.number, MASK_ALL );
		if( tr.fraction < 1 && tr.entityNum != ent->s.tracktarget ) {
			unlock(ent);
			return;
		}

		// only weapons that can lock on
		if( availableWeapons[ent->s.weaponIndex].type != WT_GUIDEDBOMB &&
			availableWeapons[ent->s.weaponIndex].type != WT_ANTIAIRMISSILE &&
			availableWeapons[ent->s.weaponIndex].type != WT_ANTIGROUNDMISSILE &&
			availableWeapons[ent->s.weaponIndex].type != WT_ANTIRADARMISSILE ) {
			if( ent->client->ps.stats[STAT_LOCKINFO] & LI_LOCKING ) {
				//unlock(ent);
				ent->client->ps.stats[STAT_LOCKINFO] &= ~LI_LOCKING;
				return;
			}
		}

		// what can we lock on ?
		targetcat = 0;
		if(	availableWeapons[ent->s.weaponIndex].type == WT_ANTIAIRMISSILE &&
			(ent->client->ps.ONOFF & OO_RADAR_AIR) ) {
			targetcat = CAT_PLANE|CAT_HELO;
		} else if( availableWeapons[ent->s.weaponIndex].type != WT_ANTIAIRMISSILE &&
			(ent->client->ps.ONOFF & OO_RADAR_GROUND) ) {
			targetcat = CAT_GROUND;
			buildings = qtrue;
		}
		if( !targetcat ) {
			if( ent->client->ps.stats[STAT_LOCKINFO] & LI_LOCKING ) {
				ent->client->ps.stats[STAT_LOCKINFO] &= ~LI_LOCKING;
				//unlock(ent);
				return;
			}
		}
		// check time
		if( level.time > ent->locktime + availableWeapons[ent->s.weaponIndex].lockdelay ) {
			ent->client->ps.stats[STAT_LOCKINFO] |= LI_LOCKING;
		}
	}
}



/* old version
void updateTargetTracking( gentity_t *ent )
{
	unsigned long	targetcat = CAT_ANY;//0;
	trace_t			tr;
	vec3_t			forward, endpos, dir;
	float			range;
	qboolean		buildings = qtrue;//qfalse;
	gentity_t		*test;

	// only weapons that can lock on
	if( availableWeapons[ent->s.weaponIndex].type != WT_GUIDEDBOMB &&
		availableWeapons[ent->s.weaponIndex].type != WT_ANTIAIRMISSILE &&
		availableWeapons[ent->s.weaponIndex].type != WT_ANTIGROUNDMISSILE &&
		availableWeapons[ent->s.weaponIndex].type != WT_ANTIRADARMISSILE ) {
		if( ent->client->ps.stats[STAT_LOCKINFO] ) unlock(ent);
		return;
	}

	// what can we lock on ?
	if(	availableWeapons[ent->s.weaponIndex].type == WT_ANTIAIRMISSILE &&
		(ent->client->ps.ONOFF & OO_RADAR_AIR) ) {
		targetcat = CAT_PLANE|CAT_HELO;
	} else if( availableWeapons[ent->s.weaponIndex].type != WT_ANTIAIRMISSILE &&
		(ent->client->ps.ONOFF & OO_RADAR_GROUND) ) {
		targetcat = CAT_GROUND;
		buildings = qtrue;
	}
	
	if( !targetcat ) {
		if( ent->client->ps.stats[STAT_LOCKINFO] ) unlock(ent);
		return;
	}

	// weapon range and vehicle direction
	range = availableWeapons[ent->s.weaponIndex].range;
	if( availableVehicles[ent->client->vehicle].id&CAT_ANY & CAT_GROUND ) {
		vec3_t	right, up, temp;
		AngleVectors( ent->client->ps.vehicleAngles, forward, right, up );
		RotatePointAroundVector( temp, up, forward, ((float)ent->client->ps.turretAngle)/10 );
		CrossProduct( up, temp, right );
		RotatePointAroundVector( dir, right, temp, ((float)ent->client->ps.gunAngle)/10 );
		VectorCopy( dir, forward );
	} else {
		VectorCopy( ent->s.angles, dir );
		AngleVectors( dir, forward, 0, 0 );
	}


	// no target yet
	if( !ent->tracktarget ) {
		VectorMA( ent->r.currentOrigin, range, forward, endpos );
		trap_Trace( &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, endpos, ent->s.number, MASK_ALL );
		// nothing found
		if( tr.entityNum == ENTITYNUM_NONE ) {
			return;
		}
		// found something
		test = &g_entities[tr.entityNum];
		if( test->s.eType == ET_VEHICLE && test->client && 
			(availableVehicles[test->client->vehicle].id&CAT_ANY & targetcat) ) {
	//		G_Printf("found target: player %s\n", test->client->pers.netname );
			track(ent, test);
		} else if( test->s.eType == ET_MISC_VEHICLE &&
			(availableVehicles[test->s.modelindex].id&CAT_ANY & targetcat) ) {
	//		G_Printf("found target: drone %s\n", test->targetname );
			track(ent, test);
		} else {
			if( buildings ) {
				if( test->s.eType == ET_EXPLOSIVE && test->takedamage ) {
					track(ent, test);
				}
			} else {
				return;
			}
		}
	} else { // update existing target
		vec3_t diff;
		float dot, dist;
		if( ent->tracktarget->s.eType == ET_EXPLOSIVE ) {
			vec3_t	mid;
			VectorAdd( ent->tracktarget->r.absmax, ent->tracktarget->r.absmin, mid );
			VectorScale( mid, 0.5f, mid );
			VectorSubtract( mid, ent->r.currentOrigin, diff );
		} else {
			VectorSubtract( ent->tracktarget->r.currentOrigin, ent->r.currentOrigin, diff );
		}
		// check within range
		dist = VectorNormalize( diff );
		if( dist > range ) {
			unlock(ent);
			return;
		}
		// check within cone
		dot = DotProduct( forward, diff );
		if( dot < availableWeapons[ent->s.weaponIndex].trackcone ) {// roughly 45 degrees 
			unlock(ent);
			return;
		} else if( dot < availableWeapons[ent->s.weaponIndex].lockcone ) { // roughly 10 degrees
			ent->locktime = level.time;
			ent->client->ps.stats[STAT_LOCKINFO] &= ~LI_LOCKING;
		}
			
		// check LOS
		VectorMA( ent->r.currentOrigin, dist, diff, endpos );
		trap_Trace( &tr, ent->r.currentOrigin, 0, 0, endpos, ent->s.number, MASK_ALL );
		if( tr.fraction < 1 && tr.entityNum != ent->s.tracktarget ) {
			unlock(ent);
			return;
		}

		// check time
		if( level.time > ent->locktime + availableWeapons[ent->s.weaponIndex].lockdelay ) {
			ent->client->ps.stats[STAT_LOCKINFO] |= LI_LOCKING;
		}
	}
}

*/
