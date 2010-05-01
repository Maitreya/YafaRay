
#include <textures/basicnodes.h>
#include <textures/layernode.h>
#include <core_api/object3d.h>
#include <core_api/camera.h>

__BEGIN_YAFRAY

textureMapper_t::textureMapper_t(const texture_t *texture): tex(texture), bumpStr(0.02), doScalar(true)
{
	map_x=1; map_y=2, map_z=3;
}

void textureMapper_t::setup()
{
	if(tex->discrete())
	{
		int u, v, w;
		tex->resolution(u, v, w);
		deltaU = 1.0/(PFLOAT)u;
		deltaV = 1.0/(PFLOAT)v;
		if(tex->isThreeD())
		{
			deltaW = 1.0/(PFLOAT)w;
			delta = fSqrt(deltaU*deltaU + deltaV*deltaV + deltaW*deltaW);
		}
		else delta = fSqrt(deltaU*deltaU + deltaV*deltaV);
	}
	else
	{
		deltaU = 0.0002;
		deltaV = 0.0002;
		deltaW = 0.0002;
		delta  = 0.0002;
	}
	PFLOAT mapScale = scale.length();
	delta /= mapScale;
	bumpStr /= mapScale;
}

// Map the texture to a cylinder
inline point3d_t tubemap(const point3d_t &p)
{
	point3d_t res;
	res.y = p.z;
	PFLOAT d = p.x*p.x + p.y*p.y;
	if (d>0) {
		res.z = 1.0/fSqrt(d);
		res.x = -atan2(p.x, p.y) * M_1_PI;
	}
	else res.x = res.z = 0;
	return res;
}

// Map the texture to a sphere
inline point3d_t spheremap(const point3d_t &p)
{
	point3d_t res(0.f);
	PFLOAT d = p.x*p.x + p.y*p.y + p.z*p.z;
	if (d>0) {
		res.z = fSqrt(d);
		if ((p.x!=0) && (p.y!=0)) res.x = -atan2(p.x, p.y) * M_1_PI;
		res.y = 1.0f - 2.0f*(acos(p.z/res.z) * M_1_PI);
	}
	return res;
}

// Map the texture to a cube
inline point3d_t cubemap(const point3d_t &p, const vector3d_t &n)
{
	const int ma[3][3] = { {1, 2, 0}, {0, 2, 1}, {0, 1, 2} };
	int axis = std::fabs(n.x) > std::fabs(n.y) ? (std::fabs(n.x) > std::fabs(n.z) ? 0 : 2) : (std::fabs(n.y) > std::fabs(n.z) ? 1 : 2);
	return point3d_t(p[ma[axis][0]], p[ma[axis][1]], p[ma[axis][2]]);
}

// Map the texture to a plane but it should not be used by now as it does nothing, it's just for completeness sake
inline point3d_t flatmap(const point3d_t &p)
{
	return p;
}

point3d_t textureMapper_t::doMapping(const point3d_t &p, const vector3d_t &N)const
{
	point3d_t texpt(p);
	// Texture coordinates standardized, if needed, to -1..1
	switch(tex_coords)
	{
		case TXC_UV:	texpt = point3d_t(2.0f*texpt.x-1.0f, 2.0f*texpt.y-1.0f, texpt.z); break;
		default: break;
	}
	// Texture axis mapping
	PFLOAT texmap[4] = {0, texpt.x, texpt.y, texpt.z};
	texpt.x=texmap[map_x];
	texpt.y=texmap[map_y];
	texpt.z=texmap[map_z];
	// Texture coordinates mapping
	switch(tex_maptype)
	{
		case TXP_TUBE:	texpt = tubemap(texpt); break;
		case TXP_SPHERE:texpt = spheremap(texpt); break;
		case TXP_CUBE:	texpt = cubemap(texpt, N); break;
		case TXP_PLAIN:	// texpt = flatmap(texpt); break; 
		default: break;
	}
	// Texture scale and offset
	texpt = mult(texpt,scale) + offset;
	
	return texpt;
}


point3d_t eval_uv(const surfacePoint_t &sp)
{
		return point3d_t(sp.U, sp.V, 0.f);
}

void textureMapper_t::eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp)const
{
	point3d_t texpt;
	vector3d_t Ng;
	// Switch texture coordinates
	switch(tex_coords)
	{
		case TXC_UV:	texpt = eval_uv(sp); Ng = sp.Ng; break;
		case TXC_ORCO:	texpt = sp.orcoP; Ng = sp.orcoNg; break;
		case TXC_TRAN:	texpt = mtx * sp.P; Ng = sp.Ng; break;
		case TXC_WIN:   texpt = state.cam->screenproject(sp.P); Ng = sp.Ng; break;
		case TXC_STICK:	// Not implemented yet use GLOB
		case TXC_STRESS:// Not implemented yet use GLOB
		case TXC_TAN:	// Not implemented yet use GLOB
		case TXC_NOR:	// Not implemented yet use GLOB
		case TXC_REFL:	// Not implemented yet use GLOB
		case TXC_GLOB:	// Nothing to do for GLOB
		default: 	texpt = sp.P; Ng = sp.Ng; break;
	}
	texpt = doMapping(texpt,Ng);
	
	stack[this->ID] = nodeResult_t(tex->getColor(texpt), (doScalar) ? tex->getFloat(texpt) : 0.f );
}

// Basically you shouldn't call this anyway, but for the sake of consistency, redirect:
void textureMapper_t::eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, const vector3d_t &wi)const
{
	eval(stack, state, sp);
}

// Normal perturbation
void textureMapper_t::evalDerivative(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp)const
{
	static bool debug=true;
	CFLOAT du, dv;
	if(tex_coords == TXC_UV)
	{
		if (tex->isNormalmap())
                {
			point3d_t p = point3d_t(sp.U, sp.V, 0.f);
			p = doMapping(p,sp.Ng);
			CFLOAT dfdu = 2*tex->getColor(p).R -1.0f;
			CFLOAT dfdv = 2*tex->getColor(p).G -1.0f;
			CFLOAT dfdw = 2*tex->getColor(p).B -1.0f;
			vector3d_t t = vector3d_t(1,0,0);
			vector3d_t b = vector3d_t(0,1,0);
			vector3d_t n = vector3d_t(0,0,1);
			vector3d_t norm = dfdu*t + dfdv*b + dfdw*n;
			// Convert norm into shading space
			vector3d_t vecU,vecV;
			createCS(sp.Ng,vecU,vecV);
			vector3d_t dSdU,dSdV;
			dSdU.x = vecU  * sp.dPdU;
			dSdU.y = vecV  * sp.dPdU;
			dSdU.z = sp.Ng * sp.dPdU;
			dSdV.x = vecU  * sp.dPdV;
			dSdV.y = vecV  * sp.dPdV;
			dSdV.z = sp.Ng * sp.dPdV;
			du = (norm * dSdU.normalize())*bumpStr;
			dv = -(norm * dSdV.normalize())*bumpStr;
		}
		else
		{
			point3d_t p1 = point3d_t(sp.U+deltaU, sp.V, 0.f);
			point3d_t p2 = point3d_t(sp.U-deltaU, sp.V, 0.f);
			p1 = doMapping(p1,sp.Ng);
			p2 = doMapping(p2,sp.Ng);
			CFLOAT dfdu = ( tex->getFloat(p1) - tex->getFloat(p2) ) / deltaU;
			p1 = point3d_t(sp.U, sp.V+deltaV, 0.f);
			p2 = point3d_t(sp.U, sp.V-deltaV, 0.f);
			p1 = doMapping(p1,sp.Ng);
			p2 = doMapping(p2,sp.Ng);
			CFLOAT dfdv = ( tex->getFloat(p1) - tex->getFloat(p2) ) / deltaV;
			// now we got the derivative in UV-space, but need it in shading space:
			vector3d_t vecU = /* deltaU * */ sp.dSdU;
			vector3d_t vecV = /* deltaV * */ sp.dSdV;
			vecU.normalize();
			vecV.normalize();
			vecU.z = dfdu;
			vecV.z = dfdv;
			// now we have two vectors NU/NV/df; Solve plane equation to get 1/0/df and 0/1/df (i.e. dNUdf and dNVdf)
			vector3d_t norm = vecU ^ vecV;
			if(std::fabs(norm.z) > 1e-30f)
			{
				PFLOAT NF = 1.0/norm.z * bumpStr * 0.01f;
				du = norm.x*NF;
				dv = norm.y*NF;
			}
			else du = dv = 0.f;
			/*if(debug)
			{
				std::cout << "deltaU:" << deltaU << ", deltaV:" << deltaV << std::endl;
				std::cout << "dfdu:" << dfdu << ", dfdv:" << dfdv << std::endl;
				std::cout << "vecU:" << vecU << ", vecV:" << vecV << ", norm:" << norm << std::endl;
				std::cout << "du:" << du << ", dv:" << dv << std::endl;
			}*/
		}
	}
	else
	{
		/* point3d_t pT1, pT2;
		vector3d_t tNU = vecToTexspace(sp.P, sp.NU);
		vector3d_t tNV = vecToTexspace(sp.P, sp.NV);
		PFLOAT l1 = tNU.length();
		PFLOAT l2 = tNV.length();
		PFLOAT d1 = delta/l1, d2 = delta/l2;
		tNU *= d1;
		tNV *= d2; */
		// todo: handle out-of-range from doRawMapping!
		// TODO: Remove redundant code
		point3d_t texpt;
		vector3d_t Ng;
		// Switch texture coordinates
		switch(tex_coords)
		{
			case TXC_ORCO:	texpt = sp.orcoP; Ng = sp.orcoNg; break;
			case TXC_TRAN:	texpt = mtx * sp.P; Ng = sp.Ng; break;
			case TXC_WIN:	texpt = state.cam->screenproject(sp.P); Ng = sp.Ng; break;
			case TXC_STICK:	// Not implemented yet use GLOB
			case TXC_STRESS:// Not implemented yet use GLOB
			case TXC_TAN:	// Not implemented yet use GLOB
			case TXC_NOR:	// Not implemented yet use GLOB
			case TXC_REFL:	// Not implemented yet use GLOB
			case TXC_GLOB:	// Nothing to do for GLOB
			default:	texpt = sp.P; Ng = sp.Ng; break;
		}
		//doMapping(...)
		//placeholder!
		du = bumpStr * ((  tex->getFloat(doMapping(texpt+delta*sp.NU, Ng))
						- tex->getFloat(doMapping(texpt-delta*sp.NU, Ng)) ) / delta);
		dv = bumpStr * ((  tex->getFloat(doMapping(texpt+delta*sp.NV, Ng))
						- tex->getFloat(doMapping(texpt-delta*sp.NV, Ng)) ) / delta);
	}
	stack[this->ID] = nodeResult_t(colorA_t(du, dv, 0.f, 0.f), 0.f );
	debug=false;
}

shaderNode_t* textureMapper_t::factory(const paraMap_t &params,renderEnvironment_t &render)
{
	const texture_t *tex=0;
	const std::string *texname=0, *option=0;
	TEX_COORDS tc = TXC_GLOB;
	TEX_PROJ maptype = TXP_PLAIN;
	float bumpStr = 1.f;
	bool scalar=true;
	int map[3] = { 1, 2, 3 };
	point3d_t offset(0.f), scale(1.f);
	matrix4x4_t mtx(1);
	if( !params.getParam("texture", texname) )
	{
		std::cerr << "[ERROR]: no texture given for texture mapper!";
		return 0;
	}
	tex = render.getTexture(*texname);
	if(!tex)
	{
		std::cerr << "[ERROR]: texture '"<<texname<<"' does not exist!";
		return 0;
	}
	textureMapper_t *tm = new textureMapper_t(tex);
	if(params.getParam("texco", option) )
	{
		if(*option == "uv") tc = TXC_UV;
		else if(*option == "global") tc = TXC_GLOB;
		else if(*option == "orco") tc = TXC_ORCO;
		else if(*option == "transformed") tc = TXC_TRAN;
		else if(*option == "window") tc = TXC_WIN;
		else if(*option == "normal") tc = TXC_NOR;
		else if(*option == "reflect") tc = TXC_REFL;
		else if(*option == "stick") tc = TXC_STICK;
		else if(*option == "stress") tc = TXC_STRESS;
		else if(*option == "tangent") tc = TXC_TAN;
	}
	if(params.getParam("mapping", option) && tex->discrete())
	{
		if(*option == "plain") maptype = TXP_PLAIN;
		else if(*option == "cube") maptype = TXP_CUBE;
		else if(*option == "tube") maptype = TXP_TUBE;
		else if(*option == "sphere") maptype = TXP_SPHERE;
	}
	params.getMatrix("transform", mtx);
	params.getParam("scale", scale);
	params.getParam("offset", offset);
	params.getParam("do_scalar", scalar);
	params.getParam("bump_strength", bumpStr);
	params.getParam("proj_x", map[0]);
	params.getParam("proj_y", map[1]);
	params.getParam("proj_z", map[2]);
	for(int i=0; i<3; ++i) map[i] = std::min(3, std::max(0, map[i]));
	tm->tex_coords = tc;
	tm->tex_maptype = maptype;
	tm->map_x = map[0];
	tm->map_y = map[1];
	tm->map_z = map[2];
	tm->scale = vector3d_t(scale);
	tm->offset = vector3d_t(2*offset);	// Offset need to be doubled due to -1..1 texture standardized wich results in a 2 wide width/height
	tm->doScalar = scalar;
	tm->bumpStr = bumpStr;
	tm->mtx = mtx;
	tm->setup();
	return tm;
}

/* ==========================================
/  The most simple node you could imagine...
/ ========================================== */

void valueNode_t::eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp)const
{
	stack[this->ID] = nodeResult_t(color, value);
}

void valueNode_t::eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, const vector3d_t &wi)const
{
	stack[this->ID] = nodeResult_t(color, value);
}

shaderNode_t* valueNode_t::factory(const paraMap_t &params,renderEnvironment_t &render)
{
	color_t col(1.f);
	float alpha=1.f;
	float val=1.f;
	params.getParam("color", col);
	params.getParam("alpha", alpha);
	params.getParam("scalar", val);
	return new valueNode_t(colorA_t(col, alpha), val);
}

/* ==========================================
/  A simple mix node, could be used to derive other math nodes
/ ========================================== */

mixNode_t::mixNode_t(): cfactor(0.f), input1(0), input2(0), factor(0)
{}

mixNode_t::mixNode_t(float val): cfactor(val), input1(0), input2(0), factor(0)
{}

void mixNode_t::eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp)const
{
	float f2 = (factor) ? factor->getScalar(stack) : cfactor;
	float f1 = 1.f - f2, fin1, fin2;
	colorA_t cin1, cin2;
	if(input1)
	{
		cin1 = input1->getColor(stack);
		fin1 = input1->getScalar(stack);
	}
	else
	{
		cin1 = col1;
		fin1 = val1;
	}
	if(input2)
	{
		cin2 = input2->getColor(stack);
		fin2 = input2->getScalar(stack);
	}
	else
	{
		cin2 = col2;
		fin2 = val2;
	}
	
	colorA_t color = f1 * cin1 + f2 * cin2;
	float   scalar = f1 * fin1 + f2 * fin2;
	stack[this->ID] = nodeResult_t(color, scalar);
}

void mixNode_t::eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp, const vector3d_t &wo, const vector3d_t &wi)const
{
	eval(stack, state, sp);
}

bool mixNode_t::configInputs(const paraMap_t &params, const nodeFinder_t &find)
{
	const std::string *name=0;
	if( params.getParam("input1", name) )
	{
		input1 = find(*name);
		if(!input1){ std::cout << "mixNode_t::configInputs: couldn't get input1 " << *name << std::endl; return false; }
	}
	else if(!params.getParam("color1", col1)){ std::cout << "mixNode_t::configInputs: color1 not set\n"; return false; }
	
	if( params.getParam("input2", name) )
	{
		input2 = find(*name);
		if(!input2){ std::cout << "mixNode_t::configInputs: couldn't get input2 " << *name << std::endl; return false; }
	}
	else if(!params.getParam("color2", col2)){ std::cout << "mixNode_t::configInputs: color2 not set\n"; return false; }
	
	if( params.getParam("factor", name) )
	{
		factor = find(*name);
		if(!factor){ std::cout << "mixNode_t::configInputs: couldn't get factor " << *name << std::endl; return false; }
	}
	else if(!params.getParam("value", cfactor)){ std::cout << "mixNode_t::configInputs: value not set\n"; return false; }
	return true;
}

bool mixNode_t::getDependencies(std::vector<const shaderNode_t*> &dep) const
{
	if(input1) dep.push_back(input1);
	if(input2) dep.push_back(input2);
	if(factor) dep.push_back(factor);
	return !dep.empty();
}

class addNode_t: public mixNode_t
{
	public:
		virtual void eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp)const
		{
			float f2, fin1, fin2;
			colorA_t cin1, cin2;
			getInputs(stack, cin1, cin2, fin1, fin2, f2);
			
			cin1 += f2 * cin2;
			fin1 += f2 * fin2;
			stack[this->ID] = nodeResult_t(cin1, fin1);
		}
};

class multNode_t: public mixNode_t
{
	public:
		virtual void eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp)const
		{
			float f1, f2, fin1, fin2;
			colorA_t cin1, cin2;
			getInputs(stack, cin1, cin2, fin1, fin2, f2);
			f1 = 1.f - f2;
			
			cin1 *= colorA_t(f1) + f2 * cin2;
			fin2 *= f1 + f2 * fin2;
			stack[this->ID] = nodeResult_t(cin1, fin1);
		}
};

class subNode_t: public mixNode_t
{
	public:
		virtual void eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp)const
		{
			float f2, fin1, fin2;
			colorA_t cin1, cin2;
			getInputs(stack, cin1, cin2, fin1, fin2, f2);
			
			cin1 -= f2 * cin2;
			fin1 -= f2 * fin2;
			stack[this->ID] = nodeResult_t(cin1, fin1);
		}
};

class screenNode_t: public mixNode_t
{
	public:
		virtual void eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp)const
		{
			float f1, f2, fin1, fin2;
			colorA_t cin1, cin2;
			getInputs(stack, cin1, cin2, fin1, fin2, f2);
			f1 = 1.f - f2;
			
			colorA_t color = colorA_t(1.f) - (colorA_t(f1) + f2 * (1.f - cin2)) * (1.f - cin1);
			CFLOAT scalar   = 1.0 - (f1 + f2*(1.f - fin2)) * (1.f -  fin1);
			stack[this->ID] = nodeResult_t(color, scalar);
		}
};

class diffNode_t: public mixNode_t
{
	public:
		virtual void eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp)const
		{
			float f1, f2, fin1, fin2;
			colorA_t cin1, cin2;
			getInputs(stack, cin1, cin2, fin1, fin2, f2);
			f1 = 1.f - f2;
			
			cin1.R = f1*cin1.R + f2*std::fabs(cin1.R - cin2.R);
			cin1.G = f1*cin1.G + f2*std::fabs(cin1.G - cin2.G);
			cin1.B = f1*cin1.B + f2*std::fabs(cin1.B - cin2.B);
			cin1.A = f1*cin1.A + f2*std::fabs(cin1.A - cin2.A);
			fin1   = f1*fin1   + f2*std::fabs(fin1 - fin2);
			stack[this->ID] = nodeResult_t(cin1, fin1);
		}
};

class darkNode_t: public mixNode_t
{
	public:
		virtual void eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp)const
		{
			float f2, fin1, fin2;
			colorA_t cin1, cin2;
			getInputs(stack, cin1, cin2, fin1, fin2, f2);
			
			cin2 *= f2;
			if(cin2.R < cin1.R) cin1.R = cin2.R;
			if(cin2.G < cin1.G) cin1.G = cin2.G;
			if(cin2.B < cin1.B) cin1.B = cin2.B;
			if(cin2.A < cin1.A) cin1.A = cin2.A;
			fin2 *= f2;
			if(fin2 < fin1) fin1 = fin2;
			stack[this->ID] = nodeResult_t(cin1, fin1);
		}
};

class lightNode_t: public mixNode_t
{
	public:
		virtual void eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp)const
		{
			float f2, fin1, fin2;
			colorA_t cin1, cin2;
			getInputs(stack, cin1, cin2, fin1, fin2, f2);
			
			cin2 *= f2;
			if(cin2.R > cin1.R) cin1.R = cin2.R;
			if(cin2.G > cin1.G) cin1.G = cin2.G;
			if(cin2.B > cin1.B) cin1.B = cin2.B;
			if(cin2.A > cin1.A) cin1.A = cin2.A;
			fin2 *= f2;
			if(fin2 > fin1) fin1 = fin2;
			stack[this->ID] = nodeResult_t(cin1, fin1);
		}
};

class overlayNode_t: public mixNode_t
{
	public:
		virtual void eval(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp)const
		{
			float f1, f2, fin1, fin2;
			colorA_t cin1, cin2;
			getInputs(stack, cin1, cin2, fin1, fin2, f2);
			f1 = 1.f - f2;
			
			colorA_t color;
			color.R = (cin1.R < 0.5f) ? cin1.R * (f1 + 2.0f*f2*cin2.R) : 1.0 - (f1 + 2.0f*f2*(1.0 - cin2.R)) * (1.0 - cin1.R);
			color.G = (cin1.G < 0.5f) ? cin1.G * (f1 + 2.0f*f2*cin2.G) : 1.0 - (f1 + 2.0f*f2*(1.0 - cin2.G)) * (1.0 - cin1.G);
			color.B = (cin1.B < 0.5f) ? cin1.B * (f1 + 2.0f*f2*cin2.B) : 1.0 - (f1 + 2.0f*f2*(1.0 - cin2.B)) * (1.0 - cin1.B);
			color.A = (cin1.A < 0.5f) ? cin1.A * (f1 + 2.0f*f2*cin2.A) : 1.0 - (f1 + 2.0f*f2*(1.0 - cin2.A)) * (1.0 - cin1.A);
			CFLOAT scalar = (fin1 < 0.5f) ? fin1 * (f1 + 2.0f*f2*fin2) : 1.0 - (f1 + 2.0f*f2*(1.0 - fin2)) * (1.0 - fin1);
			stack[this->ID] = nodeResult_t(color, scalar);
		}
};


shaderNode_t* mixNode_t::factory(const paraMap_t &params,renderEnvironment_t &render)
{
	float val=0.5f;
	int mode=0;
	params.getParam("cfactor", val);
	params.getParam("mode", mode);
	
	switch(mode)
	{
		case MN_MIX: 		return new mixNode_t(val);
		case MN_ADD: 		return new addNode_t();
		case MN_MULT: 		return new multNode_t();
		case MN_SUB: 		return new subNode_t();
		case MN_SCREEN:		return new screenNode_t();
		case MN_DIFF:		return new diffNode_t();
		case MN_DARK:		return new darkNode_t();
		case MN_LIGHT:		return new lightNode_t();
		case MN_OVERLAY:	return new overlayNode_t();
	}
	return new mixNode_t(val);
}

// ==================

extern "C"
{
	YAFRAYPLUGIN_EXPORT void registerPlugin(renderEnvironment_t &render)
	{
		render.registerFactory("texture_mapper", textureMapper_t::factory);
		render.registerFactory("value", valueNode_t::factory);
		render.registerFactory("mix", mixNode_t::factory);
		render.registerFactory("layer", layerNode_t::factory);
	}
}

__END_YAFRAY