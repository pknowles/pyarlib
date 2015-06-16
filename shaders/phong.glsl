
//may be set by the application (pyarlib user)
#define TANGENT_SPACE 0

#ifdef OVERRIDE_TANGENT_SPACE
#undef TANGENT_SPACE
#define TANGENT_SPACE OVERRIDE_TANGENT_SPACE
#endif

//set by the application (pyarlib internal)
#define HAS_GEOMETRY 0

#if VERTEX

//suffix all out parameters with V if going via geometry shader
#if HAS_GEOMETRY
#define esNorm esNormV
#define esLight esLightV
#define esFrag esFragV
#define tsFrag tsFragV
#define tsLight tsLightV
#define coord coordV
#endif

#if TANGENT_SPACE
out vec3 tsFrag, tsLight, tsNorm;
vec3 esNorm, esLight;
#else
out vec3 esNorm, esLight;
#endif
out vec3 esFrag;
out vec2 coord;

uniform int textured;

void phong()
{
	vec4 esVert = modelviewMat * osVert;
	vec4 csVert = projectionMat * esVert;
	gl_Position = csVert;
	
	coord = texCoord;
	
	esNorm = normalize(normalMat * osNorm);
	esFrag = esVert.xyz;
	esLight = lightPos.xyz;
	
	if (lightPos.w != 0.0)
		esLight -= esVert.xyz;
	
#if TANGENT_SPACE
	vec3 T = mat3(modelviewMat) * osTangent;
	vec3 N = esNorm;
	vec3 B = normalize(cross(T, N));
	T = normalize(cross(B, N));
	mat3 TBN = transpose(mat3(T,B,N));

	if ((textured & 2) != 0)
	{
		tsLight = TBN * esLight;
		tsFrag = TBN * esFrag;
		tsNorm = vec3(0,0,1);
		tsNorm = T;
	}
	else
	{
		tsLight = esLight;
		tsFrag = esFrag;
		tsNorm = esNorm;
	}
#endif
}

#endif

#if GEOMETRY

//passthrough only - simply substitute names
#if TANGENT_SPACE
#define geomPassthroughOneV tsFragV
#define geomPassthroughTwoV tsLightV
#define geomPassthroughOne tsFrag
#define geomPassthroughTwo tsLight
#define geomPassthroughThree tsNorm
#else
#define geomPassthroughOneV esLightV
#define geomPassthroughTwoV esNormV
#define geomPassthroughOne esLight
#define geomPassthroughTwo esNorm
#define geomPassthroughThree tsNorm
#endif

in vec3 geomPassthroughOneV[3];
in vec3 geomPassthroughTwoV[3];
in vec3 geomPassthroughThreeV[3];
in vec3 esFragV[3];
in vec2 coordV[3];

out vec3 geomPassthroughOne;
out vec3 geomPassthroughTwo;
out vec3 geomPassthroughThree;
out vec3 esFrag;
out vec2 coord;

void phong(int i)
{
	geomPassthroughOne = geomPassthroughOneV[i];
	geomPassthroughTwo = geomPassthroughTwoV[i];
	esFrag = esFragV[i];
	coord = coordV[i];
}

#endif

#if FRAGMENT

#if TANGENT_SPACE
in vec3 tsFrag, tsLight;
in vec3 tsNorm;
#else
in vec3 esNorm, esLight;
#endif
in vec3 esFrag;
in vec2 coord;

uniform vec4 colourIn;
uniform vec3 colAmbient;
uniform vec3 colSpecular;

uniform sampler2D texColour;
uniform sampler2D texNormal;
uniform sampler2D texSpecular;

uniform int textured;
uniform int gloss;
uniform int unlit;

vec4 phong()
{
	vec4 fragColour;
	
	float specScale = 1.0;
	float specPower = 50.0;
	
	
#if TANGENT_SPACE
	vec3 L = normalize(tsLight);
	vec3 E = normalize(-tsFrag);
	vec3 N = normalize(tsNorm);

	if ((textured & 2) != 0)
	{
		N = texture(texNormal, coord).xyz * 2.0 - 1.0;
		//N.y = -N.y;
		//N.x = -N.x;
		N = normalize(N);
	}
#else
	vec3 L = normalize(esLight);
	vec3 E = normalize(-esFrag);
	vec3 N = normalize(esNorm);
#endif

	if ((textured & 4) != 0)
	{
		specScale = texture(texSpecular, coord).r;
		if (gloss == 1)
			specPower = texture(texSpecular, coord).g * 255.0;
	}

	#ifndef DIFFUSE_COLOUR
	#define DIFFUSE_COLOUR ((((textured & 1) != 0) ? texture(texColour, coord) : vec4(1.0))  * colourIn)
	#endif
	fragColour = DIFFUSE_COLOUR;
	
	if (unlit == 0)
	{
		#ifdef BACKLIT
		float diffuse = abs(dot(N, L));
		#else
		float diffuse = max(0.0, dot(N, L));
		#endif
		
		#ifdef DIFFUSE_INCREASE
		diffuse = mix(diffuse, 1.0, DIFFUSE_INCREASE);
		#endif
		
		fragColour.rgb = fragColour.rgb * colAmbient + fragColour.rgb * diffuse;
		
		if (diffuse > 0.0)
		{
			vec3 R = reflect(-L, N);
			float specular = pow(max(0.0, dot(R, E)), specPower);
			fragColour.rgb += colSpecular * (specular * specScale * fragColour.a);
		}
	}
	else
		fragColour.rgb *= colourIn.rgb;
	
	//fragColour.rgb = N*0.5+0.5;
	//fragColour.rgb = L*0.5+0.5;
	//fragColour.rgb = E*0.5+0.5;
	//fragColour.rgb = vec3(diffuse);
	//fragColour.rgb = tsNorm*0.5+0.5;
	
	return fragColour;
}

#endif


