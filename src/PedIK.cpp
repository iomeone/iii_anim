#include "iii_anim.h"

WRAPPER void __stdcall CPedIK::ExtractYawAndPitchLocal(RwMatrixTag *, float *, float *) { EAXJMP(0x4ED2C0); }
WRAPPER void __stdcall CPedIK::ExtractYawAndPitchWorld(RwMatrixTag *, float *, float *) { EAXJMP(0x4ED140); }
WRAPPER RwMatrix *CPedIK::GetWorldMatrix(RwFrame *, RwMatrixTag *) { EAXJMP(0x4ED060); }
WRAPPER int __stdcall CPedIK::MoveLimb(LimbOrientation *, float, float, float *) { EAXJMP(0x4ED440); }
float *CPedIK::ms_headInfo = (float*)0x5F9F5C;
float *CPedIK::ms_headRestoreInfo = (float*)0x5F9F74;
float *CPedIK::ms_torsoInfo = (float*)0x5F9F8C;
float *CPedIK::ms_upperArmInfo = (float*)0x5F9FA4;
float *CPedIK::ms_lowerArmInfo = (float*)0x5F9FBC;

WRAPPER double CGeneral__LimitRadianAngle(float) { EAXJMP(0x48CB90); }

const RwV3d xAxis = { 1.0f, 0.0f, 0.0f};
const RwV3d yAxis = { 0.0f, 1.0f, 0.0f};
const RwV3d zAxis = { 0.0f, 0.0f, 1.0f};

void
CPedIK::GetComponentPosition(RwV3d *pos, int id)
{
	RwFrame *f;
	RwMatrix *mat;

	if(IsClumpSkinned(this->ped->clump)){
		pos->x = 0.0f;
		pos->y = 0.0f;
		pos->z = 0.0f;
		RpHAnimHierarchy *hier = GetAnimHierarchyFromSkinClump(this->ped->clump);
		RwInt32 idx = RpHAnimIDGetIndex(hier, this->ped->frames[id]->nodeID);
		RwMatrix *mats = RpHAnimHierarchyGetMatrixArray(hier);
		RwV3dTransformPoints(pos, pos, 1, &mats[idx]);
	}else{
		f = this->ped->frames[id]->frame;
		mat = &f->modelling;
		*pos = mat->pos;
		for(f = (RwFrame*)f->object.parent; f; f = (RwFrame*)f->object.parent)
			RwV3dTransformPoints(pos, pos, 1, &f->modelling);
	}
}

void
CPedIK::ExtractYawAndPitchLocalSkinned(AnimBlendFrameData *frameData, float *yaw, float *pitch)
{
	RwMatrix *mat = RwMatrixCreate();
	RtQuatConvertToMatrix(&frameData->hanimframe->q, mat);
	CPedIK::ExtractYawAndPitchLocal(mat, yaw, pitch);
	RwMatrixDestroy(mat);
}

void
CPedIK::RotateHead(void)
{
	RtQuat *q = &this->ped->frames[PED_HEAD]->hanimframe->q;
	RtQuatRotate(q, &xAxis, RAD2DEG(this->headOrient.phi), rwCOMBINEREPLACE);
	RtQuatRotate(q, &zAxis, RAD2DEG(this->headOrient.theta), rwCOMBINEPOSTCONCAT);
	this->ped->bfFlagsI |= 0x20;
}

int
CPedIK::LookInDirection(float phi, float theta)
{
	int ret;
	ret = 1;

	RwMatrix mat;
	AnimBlendFrameData *frameData = this->ped->frames[PED_HEAD];
	float alpha, beta;
	if(IsClumpSkinned(this->ped->clump)){
		if((frameData->flag & 2) == 0){
			frameData->flag |= 2;
			CPedIK::ExtractYawAndPitchLocalSkinned(frameData, &this->headOrient.phi, &this->headOrient.theta);
		}
		RpHAnimHierarchy *hier = GetAnimHierarchyFromSkinClump(this->ped->clump);
		RwInt32 idx = RpHAnimIDGetIndex(hier, 8);
		mat = RpHAnimHierarchyGetMatrixArray(hier)[idx];
		CPedIK::ExtractYawAndPitchWorld(&mat, &alpha, &beta);

		int foo = CPedIK::MoveLimb(&this->headOrient, CGeneral__LimitRadianAngle(phi - alpha),
		                           CGeneral__LimitRadianAngle(0.17453294), CPedIK::ms_headInfo);
		if(foo == 0)
			ret = 0;
		if(foo != 2){
			if(this->flags & 2){
				this->RotateHead();
				return ret;
			}else if(CPedIK::MoveLimb(&this->torsoOrient, CGeneral__LimitRadianAngle(phi),
					            theta, CPedIK::ms_torsoInfo))
				ret = 1;
		}
		if((this->flags & 2) == 0)
			this->RotateTorso(this->ped->frames[PED_TORSO], &this->torsoOrient, false);
		this->RotateHead();
	}else{
		RwFrame *f = frameData->frame;
		if((frameData->flag & 2) == 0){
			frameData->flag |= 2;
			CPedIK::ExtractYawAndPitchLocal(&f->modelling, &this->headOrient.phi, &this->headOrient.theta);
		}
		RwMatrix *worldMat = CPedIK::GetWorldMatrix((RwFrame*)f->object.parent, RwMatrixCreate());
		CPedIK::ExtractYawAndPitchWorld(worldMat, &alpha, &beta);
		RwMatrixDestroy(worldMat);

		alpha = CGeneral__LimitRadianAngle(phi - (alpha + this->torsoOrient.phi));
		beta = CGeneral__LimitRadianAngle(theta - beta*cos(alpha));
		int foo = CPedIK::MoveLimb(&this->headOrient, alpha, beta, CPedIK::ms_headInfo);
		if(foo == 0)
			ret = 0;
		if(foo != 2 && (this->flags & 2) == 0){
			if(CPedIK::MoveLimb(&this->torsoOrient, CGeneral__LimitRadianAngle(phi - this->ped->fRotationCur),
				            theta, CPedIK::ms_torsoInfo))
				ret = 1;
		}
		CMatrix cmat;
		cmat.ctor(&f->modelling, false);
		RwV3d pos = cmat.matrix.pos;
		cmat.SetRotateZ(this->headOrient.theta);
		cmat.RotateX(this->headOrient.phi);
		cmat.matrix.pos.x += pos.x;
		cmat.matrix.pos.y += pos.y;
		cmat.matrix.pos.z += pos.z;
		cmat.UpdateRW();
		if((this->flags & 2) == 0)
			this->RotateTorso(this->ped->frames[PED_TORSO], &this->torsoOrient, false);
		cmat.dtor();
	}
	return ret;
}

bool
CPedIK::RestoreLookAt(void)
{
	bool ret = false;
	AnimBlendFrameData *frm = this->ped->frames[PED_HEAD];
	RwMatrix *mat = &frm->frame->modelling;

	float yaw, pitch;
	if(IsClumpSkinned(this->ped->clump)){
		if(frm->flag & 2)
			frm->flag &= ~2;
		else{
			CPedIK::ExtractYawAndPitchLocalSkinned(frm, &yaw, &pitch);
			if(CPedIK::MoveLimb(&this->headOrient, yaw, pitch, CPedIK::ms_headRestoreInfo) == 2)
				ret = true;
		}
		this->RotateHead();
	}else{
		if(frm->flag & 2)
			frm->flag &= ~2;
		else{
			CPedIK::ExtractYawAndPitchLocal(mat, &yaw, &pitch);
			if(CPedIK::MoveLimb(&this->headOrient, yaw, pitch, CPedIK::ms_headRestoreInfo) == 2)
				ret = true;
		}
		CMatrix cmat;
		cmat.ctor(mat, false);
		RwV3d pos = cmat.matrix.pos;
		cmat.SetRotateZ(this->headOrient.theta);
		cmat.RotateX(this->headOrient.phi);
		cmat.matrix.pos.x += pos.x;
		cmat.matrix.pos.y += pos.y;
		cmat.matrix.pos.z += pos.z;
		cmat.UpdateRW();
		cmat.dtor();
	}
	if((this->flags & 2) == 0)
		CPedIK::MoveLimb(&this->torsoOrient, 0.0f, 0.0f, CPedIK::ms_torsoInfo);
	if((this->flags & 2) == 0)
		this->RotateTorso(this->ped->frames[PED_TORSO], &this->torsoOrient, false);
	return ret;
}

void
CPedIK::RotateTorso(AnimBlendFrameData *animBlend, LimbOrientation *limb, bool flag)
{
	if(IsClumpSkinned(this->ped->clump)){
		RtQuat *q = &animBlend->hanimframe->q;
		// this is what the game does (also VC), but it does not look great
		//RtQuatRotate(q, &xAxis, RAD2DEG(limb->phi), rwCOMBINEPRECONCAT);
		//RtQuatRotate(q, &zAxis, RAD2DEG(limb->theta), rwCOMBINEPRECONCAT);	// pitch

		// copied the code from the non-skinned case
		// this seems to work ok
		RpHAnimHierarchy *hier = GetAnimHierarchyFromSkinClump(ped->clump);
		// We can't get the parent matrix but right now we have Smid hardcoded as torso
		// whose parent is Swaist
		//int idx = RpHAnimIDGetIndex(hier, BONE_Swaist);
		// or rather Storso and Smid
		int idx = RpHAnimIDGetIndex(hier, BONE_Smid);

		// Maybe this matrix isn't what we want?
		RwMatrix *mat = &RpHAnimHierarchyGetMatrixArray(hier)[idx];
		RwV3d vec1, vec2, vec3;
		vec1.x = mat->right.z;
		vec1.y = mat->up.z;
		vec1.z = mat->at.z;
		float c = cos(ped->fRotationCur);
		float s = sin(ped->fRotationCur);
		vec2.x = -(c*mat->right.x + s*mat->right.y);
		vec2.y = -(c*mat->up.x + s*mat->up.y);
		vec2.z = -(c*mat->at.x + s*mat->at.y);

		// Not sure what exactly to do here

	//	RtQuatRotate(q, &vec1, RAD2DEG(limb->phi), rwCOMBINEREPLACE);	// this is what VC does
	//	RtQuatRotate(q, &vec2, RAD2DEG(limb->theta), rwCOMBINEPRECONCAT);

		RtQuatRotate(q, &vec1, RAD2DEG(limb->phi), rwCOMBINEPRECONCAT);
		RtQuatRotate(q, &vec2, RAD2DEG(limb->theta), rwCOMBINEPRECONCAT);

	//	RtQuatRotate(q, &vec2, RAD2DEG(limb->theta), rwCOMBINEPOSTCONCAT);
	//	RtQuatRotate(q, &vec1, RAD2DEG(limb->phi), rwCOMBINEPOSTCONCAT);

		this->ped->bfFlagsI |= 0x20;
		return;
	}



	RwFrame *f = animBlend->frame;
	RwMatrix *mat = CPedIK::GetWorldMatrix((RwFrame*)f->object.parent, RwMatrixCreate());
	RwV3d vec1, vec2, vec3;
	// up vector
	vec1.x = mat->right.z;
	vec1.y = mat->up.z;
	vec1.z = mat->at.z;
	RwV3d pos = f->modelling.pos;
	// rotation == 0 -> looking in y direction
	// left? vector
	float c = cos(ped->fRotationCur);
	float s = sin(ped->fRotationCur);
	vec2.x = -(c*mat->right.x + s*mat->right.y);
	vec2.y = -(c*mat->up.x + s*mat->up.y);
	vec2.z = -(c*mat->at.x + s*mat->at.y);

	if(flag){
		CVector v1, v2, v3;
		v1.x = mat->up.x;
		v1.y = mat->up.y;
		v1.z = mat->up.z;
		v2.x = 0.0f;
		v2.y = 0.0f;
		v2.z = 1.0f;
		CrossProduct(&v3, &v2, &v1);
		v3.Normalize();
		float dot = mat->at.x*v3.x + mat->at.y*v3.y + mat->at.z*v3.z;
		if(dot > 1.0f) dot = 1.0f;
		if(dot < -1.0f) dot = -1.0f;
		float alpha = acos(dot);
		if(mat->at.z < 0.0f)
			alpha = -alpha;
		float c = cos(ped->fRotationCur);
		float s = sin(ped->fRotationCur);
		vec3.x = s * mat->right.x - c * mat->right.y;
		vec3.y = s * mat->up.x - c * mat->up.y;
		vec3.z = s * mat->at.x - c * mat->at.y;
		float a, b;
		CPedIK::ExtractYawAndPitchWorld(mat, &a, &b);
		RwMatrixRotate(&f->modelling, &vec2, RAD2DEG(limb->theta), rwCOMBINEPOSTCONCAT);
		RwMatrixRotate(&f->modelling, &vec1, RAD2DEG(limb->phi - (a - ped->fRotationCur)), rwCOMBINEPOSTCONCAT);
		RwMatrixRotate(&f->modelling, &vec3, RAD2DEG(alpha), rwCOMBINEPOSTCONCAT);
	}else{
		// pitch
		RwMatrixRotate(&f->modelling, &vec2, RAD2DEG(limb->theta), rwCOMBINEPOSTCONCAT);
		// yaw
		RwMatrixRotate(&f->modelling, &vec1, RAD2DEG(limb->phi), rwCOMBINEPOSTCONCAT);
	}
	f->modelling.pos = pos;
	RwMatrixDestroy(mat);
}

bool
CPedIK::PointGunInDirectionUsingArm(float phi, float theta)
{
	bool ret = false;
	RwMatrix *mat;
	float alpha, beta;
	RwV3d vec1, vec2, pos;
	if(IsClumpSkinned(this->ped->clump)){
		RpHAnimHierarchy *hier = GetAnimHierarchyFromSkinClump(this->ped->clump);
		mat = RwMatrixCreate();
		// PED_Shead? really?
		RwInt32 idx = RpHAnimIDGetIndex(hier, this->ped->frames[PED_HEAD]->nodeID);
		*mat = RpHAnimHierarchyGetMatrixArray(hier)[idx];
		CPedIK::ExtractYawAndPitchWorld(mat, &alpha, &beta);
		RwMatrixDestroy(mat);
	}else{
		RwFrame *f = this->ped->frames[PED_UPPERARMR]->frame;
		mat = CPedIK::GetWorldMatrix((RwFrame*)f->object.parent, RwMatrixCreate());
		CPedIK::ExtractYawAndPitchWorld(mat, &alpha, &beta);
		RwMatrixDestroy(mat);
	}
	vec1.x = 0.0f;
	vec1.y = 0.0f;
	vec1.z = 1.0f;
	if(IsClumpSkinned(this->ped->clump))
		theta += 0.17453294f;
	else{
		phi = (phi - this->torsoOrient.phi) - 0.2617994f;
		theta = CGeneral__LimitRadianAngle(theta-beta);
	}

	int flag = CPedIK::MoveLimb(&this->upperArmOrient, phi, theta, CPedIK::ms_upperArmInfo);
	if(flag == 2){
		this->flags |= 1;
		ret = true;
	}
	if(flag == 0){
		// only on PC and PS2...wtf?
	}
	if(IsClumpSkinned(this->ped->clump)){
		RtQuat *q = &this->ped->frames[PED_UPPERARMR]->hanimframe->q;
		RtQuatRotate(q, &xAxis, RAD2DEG(this->upperArmOrient.phi), rwCOMBINEPOSTCONCAT);
		RtQuatRotate(q, &zAxis, RAD2DEG(this->upperArmOrient.theta), rwCOMBINEPOSTCONCAT);
		this->ped->bfFlagsI |= 0x20;
	}else{
		RwFrame *f = this->ped->frames[PED_UPPERARMR]->frame;
		pos = f->modelling.pos;

		mat = CPedIK::GetWorldMatrix((RwFrame*)f->object.parent, RwMatrixCreate());
		vec2.x = mat->right.z;
		vec2.y = mat->up.z;
		vec2.z = mat->at.z;
		RwMatrixDestroy(mat);

		RwMatrixRotate(&f->modelling, &vec1, RAD2DEG(this->upperArmOrient.theta), rwCOMBINEPOSTCONCAT);
		RwMatrixRotate(&f->modelling, &vec2, RAD2DEG(this->upperArmOrient.phi), rwCOMBINEPOSTCONCAT);
		f->modelling.pos = pos;
	}
	return ret;
}

byte &camIdx = *(byte*)0x6FAD6E;
byte *TheCamera = (byte*)0x6FACF8;

struct CCam
{
	bool Using3rdPersonMouseCam(void);
};

WRAPPER bool CCam::Using3rdPersonMouseCam(void) { EAXJMP(0x457460); }

bool
CPedIK::PointGunInDirection(float phi, float theta)
{
	bool ret = true;
	char flag = 0;
	phi = CGeneral__LimitRadianAngle(phi - this->ped->fRotationCur);
	this->flags &= ~1;
	this->flags |= 2;
	if(this->flags & 4){
		flag = this->PointGunInDirectionUsingArm(phi, theta);
		phi = CGeneral__LimitRadianAngle(phi - this->ped->fRotationCur);
	}
	if(flag){
		if(this->flags & 4 && this->torsoOrient.phi * this->upperArmOrient.phi < 0.0f)
			CPedIK::MoveLimb(&this->torsoOrient, 0.0f, this->torsoOrient.theta, CPedIK::ms_torsoInfo);
	}else{
/*		// WTF does that even do?
		float alpha, beta;
		RwMatrix *mat = CPedIK::GetWorldMatrix((RwFrame*)this->ped->frames[PED_Supperarmr]->frame->object.parent,
		                                       RwMatrixCreate());
		CPedIK::ExtractYawAndPitchWorld(mat, &alpha, &beta);
		RwMatrixDestroy(mat);
*/
		flag = CPedIK::MoveLimb(&this->torsoOrient, phi, theta, CPedIK::ms_torsoInfo);
		if(flag){
			if(flag == 2)
				this->flags |= 1;
		}else
			ret = 0;
	}
	flag = this->flags & 4 && ((CCam*)(TheCamera + 420 * camIdx + 420))->Using3rdPersonMouseCam();
	this->RotateTorso(this->ped->frames[PED_TORSO], &this->torsoOrient, flag);
	return ret;
}

void
pedikhooks(void)
{
	InjectHook(0x4ED0F0, &CPedIK::GetComponentPosition, PATCH_JUMP);
	InjectHook(0x4ED620, &CPedIK::LookInDirection, PATCH_JUMP);
	InjectHook(0x4ED810, &CPedIK::RestoreLookAt, PATCH_JUMP);
	InjectHook(0x4EDDB0, &CPedIK::RotateTorso, PATCH_JUMP);
	InjectHook(0x4ED9B0, &CPedIK::PointGunInDirection, PATCH_JUMP);
	InjectHook(0x4EDB20, &CPedIK::PointGunInDirectionUsingArm, PATCH_JUMP);

	//MemoryVP::Patch<BYTE>(0x4ED0F0, 0xcc);	// CPedIK::GetComponentPosition		done
	//MemoryVP::Patch<BYTE>(0x4EDDB0, 0xcc);	// CPedIK::RotateTorso			done
	//MemoryVP::Patch<BYTE>(0x4EDD70, 0xcc);	// CPedIK::RestoreGunPosn		no changes
	//MemoryVP::Patch<BYTE>(0x4EDB20, 0xcc);	// CPedIK::PointGunInDirectionUsingArm	done
	//MemoryVP::Patch<BYTE>(0x4ED9B0, 0xcc);	// CPedIK::PointGunInDirection		done
	//MemoryVP::Patch<BYTE>(0x4ED810, 0xcc);	// CPedIK::RestoreLookAt		done
	//MemoryVP::Patch<BYTE>(0x4ED620, 0xcc);	// CPedIK::LookInDirection		done
	//MemoryVP::Patch<BYTE>(0x4ED590, 0xcc);	// CPedIK::LookAtPosition		no changes
}