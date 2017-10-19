#ifdef FIXME

#include "CDXShader.h"

CDXShader::CDXShader()
{
	m_pEffect = NULL;
	m_hAmbient = NULL;
	m_hDiffuse = NULL;
	m_hSpecular = NULL;
	m_hOpacity = NULL;
	m_hSpecularPower = NULL;
	m_hLightColor = NULL;
	m_hLightPosition = NULL;
	m_hCameraPosition = NULL;
	m_hTexture = NULL;
	m_hTime = NULL;
	m_hWorld = NULL;
	m_hWorldViewProjection = NULL;
	m_hTransform = NULL;
	m_hWireframeColor = NULL;
}

void CDXShader::Release()
{
	if(m_pEffect) {
		m_pEffect->Release();
		m_pEffect = NULL;
	}
}

void CDXShader::CreateEffect(LPDIRECT3DDEVICE9 pDevice)
{
	/*const char* g_strBuffer = "float3 g_vMaterialAmbient : Ambient = float3( 0.2f, 0.2f, 0.2f );   // Material's ambient color\r\n"
		"float3 g_vMaterialDiffuse : Diffuse = float3( 1.0f, 1.0f, 1.0f );   // Material's diffuse color\r\n"
		"float3 g_vMaterialSpecular : Specular = float3( 1.0f, 1.0f, 1.0f );  // Material's specular color\r\n"
		"float  g_fMaterialAlpha : Opacity = 1.0f;\r\n"
		"int    g_nMaterialShininess : SpecularPower = 32;\r\n"
		"float3 g_vLightColor : LightColor = float3( 1.0f, 1.0f, 1.0f );        // Light color\r\n"
		"float3 g_vLightPosition : LightPosition = float3( 50.0f, 10.0f, 0.0f );   // Light position\r\n"
		"float3 g_vCameraPosition : CameraPosition;\r\n"
		"texture  g_MeshTexture : Texture;   // Color texture for mesh\r\n"
		"float    g_fTime : Time;            // App's time in seconds\r\n"
		"float4x4 g_mWorld : World;          // World matrix\r\n"
		"float4x4 g_mWorldViewProjection : WorldViewProjection; // World * View * Projection matrix\r\n"
		"sampler MeshTextureSampler = \r\n"
		"	sampler_state\r\n"
		"{\r\n"
		"	Texture = <g_MeshTexture>;\r\n"
		"	MipFilter = LINEAR;\r\n"
		"	MinFilter = LINEAR;\r\n"
		"	MagFilter = LINEAR;\r\n"
		"};\r\n"
		"void Projection( float4 vPosObject: POSITION,\r\n"
		"	float3 vNormalObject: NORMAL,\r\n"
		"	float2 vTexCoordIn: TEXCOORD0,\r\n"
		"	out float4 vPosProj: POSITION,\r\n"
		"	out float2 vTexCoordOut: TEXCOORD0,\r\n"
		"	out float4 vColorOut: COLOR0,\r\n"
		"	uniform bool bSpecular\r\n"
		"	)\r\n"
		"{\r\n"
		"	float4 vPosWorld = mul( vPosObject, g_mWorld );\r\n"
		"	vPosProj = mul( vPosObject, g_mWorldViewProjection );\r\n"
		"	float3 vNormalWorld = mul( vNormalObject, (float3x3)g_mWorld );\r\n"
		"	vTexCoordOut = vTexCoordIn;\r\n"
		"	float3 vLight = normalize( g_vLightPosition - vPosWorld.xyz );\r\n"
		"	vColorOut.rgb = g_vLightColor * g_vMaterialAmbient;\r\n"
		"	vColorOut.rgb += g_vLightColor * g_vMaterialDiffuse * saturate( dot( vLight, vNormalWorld ) );\r\n"
		"	if( bSpecular )\r\n"
		"	{\r\n"
		"		float3 vCamera = normalize(vPosWorld.xyz - g_vCameraPosition);\r\n"
		"		float3 vReflection = reflect( vLight, vNormalWorld );\r\n"
		"		float  fPhongValue = saturate( dot( vReflection, vCamera ) );\r\n"
		"		vColorOut.rgb += g_vMaterialSpecular * pow(fPhongValue, g_nMaterialShininess);\r\n"
		"	}\r\n"
		"	vColorOut.a = g_fMaterialAlpha;\r\n"
		"}\r\n"
		"void Lighting( float2 vTexCoord: TEXCOORD0,\r\n"
		"	float4 vColorIn: COLOR0,\r\n"
		"	out float4 vColorOut: COLOR0,\r\n"
		"	uniform bool bTexture )\r\n"
		"{\r\n"
		"	vColorOut = vColorIn;\r\n"
		"	if( bTexture )\r\n"
		"		vColorOut.rgb *= tex2D( MeshTextureSampler, vTexCoord );\r\n"
		"}\r\n"
		"technique Specular\r\n"
		"{\r\n"
		"	pass P0\r\n"
		"	{\r\n"
		"		VertexShader = compile vs_2_0 Projection(true);\r\n"
		"		PixelShader = compile ps_2_0 Lighting(false);\r\n"
		"	}\r\n"
		"}\r\n"
		"technique NoSpecular\r\n"
		"{\r\n"
		"	pass P0\r\n"
		"	{\r\n"
		"		VertexShader = compile vs_2_0 Projection(false);\r\n"
		"		PixelShader = compile ps_2_0 Lighting(false);\r\n"
		"	}\r\n"
		"}\r\n"
		"technique TexturedSpecular\r\n"
		"{\r\n"
		"	pass P0\r\n"
		"	{\r\n"
		"		VertexShader = compile vs_2_0 Projection(true);\r\n"
		"		PixelShader = compile ps_2_0 Lighting(true);\r\n"
		"	}\r\n"
		"}\r\n"
		"technique TexturedNoSpecular\r\n"
		"{\r\n"
		"	pass P0\r\n"
		"	{\r\n"
		"		VertexShader = compile vs_2_0 Projection(false);\r\n"
		"		PixelShader = compile ps_2_0 Lighting(true);\r\n"
		"	}\r\n"
		"}\r\n";*/
	const char* g_strBuffer = "float3 g_vMaterialAmbient : Ambient = float3( 0.2f, 0.2f, 0.2f );   // Material's ambient color\r\n"
		"float3 g_vMaterialDiffuse : Diffuse = float3( 1.0f, 1.0f, 1.0f );   // Material's diffuse color\r\n"
		"float3 g_vMaterialSpecular : Specular = float3( 1.0f, 1.0f, 1.0f );  // Material's specular color\r\n"
		"float  g_fMaterialAlpha : Opacity = 1.0f;\r\n"
		"float  g_fMaterialAlphaThreshold : AlphaThreshold = 0.0f;\r\n"
		"int    g_nMaterialShininess : SpecularPower = 32;\r\n"
		"float3 g_vLightColor : LightColor = float3( 1.0f, 1.0f, 1.0f );        // Light color\r\n"
		"float3 g_vLightPosition : LightPosition = float3( 0.0f, -100.0f, 0.0f );   // Light position\r\n"
		"float3 g_vCameraPosition : CameraPosition;\r\n"
		"float3 g_vWireframe : WireframeColor = float3( 1.0f, 1.0f, 1.0f);\r\n"
		"texture  g_MeshTexture : Texture;   // Color texture for mesh\r\n"
		"float    g_fTime : Time;            // App's time in seconds\r\n"
		"float4x4 g_mWorld : World;          // World matrix\r\n"
		"float4x4 g_mWorldViewProjection : WorldViewProjection; // World * View * Projection matrix\r\n"
		"float4x4	g_mTransform: Transform;\r\n"
		"sampler DiffuseSampler = \r\n"
		"	sampler_state\r\n"
		"{\r\n"
		"	Texture = <g_MeshTexture>;\r\n"
		"	MipFilter = LINEAR;\r\n"
		"	MinFilter = LINEAR;\r\n"
		"	MagFilter = LINEAR;\r\n"
		"	AddressU = WRAP;\r\n"
		"	AddressV = WRAP;\r\n"
		"	AddressW = WRAP;\r\n"
		"};\r\n"
		"void Basic( float4 vPosObject: POSITION0,\r\n"
		"	float3 vNormalObject: NORMAL0,\r\n"
		"	float2 vTexCoordIn: TEXCOORD0,\r\n"
		"	float4 vColorIn: COLOR0,\r\n"
		"	float4 vColorSelect: COLOR1,\r\n"
		"	out float4 vPosProj: POSITION,\r\n"
		"	out float2 vTexCoordOut: TEXCOORD0,\r\n"
		"	out float4 vColorOut: COLOR0,\r\n"
		"	uniform float fLine )\r\n"
		"{\r\n"
		"	float4 vPosTransform = mul(vPosObject, g_mTransform);\r\n"
		"	vPosTransform.xyz *= fLine;\r\n"
		"	vPosProj = mul( vPosTransform, g_mWorldViewProjection );\r\n"
		"	vTexCoordOut = vTexCoordIn;\r\n"
		"	vColorOut.a = g_fMaterialAlpha;\r\n"
		"	vColorOut.rgb = g_vWireframe;\r\n"
		"}\r\n"
		"void Projection( float4 vPosObject: POSITION0,\r\n"
		"	float3 vNormalObject: NORMAL0,\r\n"
		"	float2 vTexCoordIn: TEXCOORD0,\r\n"
		"	float4 vColorIn: COLOR0,\r\n"
		"	out float4 vPosProj: POSITION,\r\n"
		"	out float2 vTexCoordOut: TEXCOORD0,\r\n"
		"	out float4 vColorOut: COLOR0,\r\n"
		"	uniform bool bSpecular )\r\n"
		"{\r\n"
		"	float4 vPosTransform = mul(vPosObject, g_mTransform);\r\n"
		"	float4 vPosWorld = mul( vPosTransform, g_mWorld );\r\n"
		"	vPosProj = mul( vPosTransform, g_mWorldViewProjection );\r\n"
		"	float3 vNormalWorld = mul( vNormalObject, (float3x3)g_mWorld );\r\n"
		"	vTexCoordOut = vTexCoordIn;\r\n"
		"	float3 vLight = normalize( g_vLightPosition - vPosWorld.xyz );\r\n"
		"	vColorOut.rgb = g_vLightColor * g_vMaterialAmbient;\r\n"
		"	vColorOut.rgb += g_vLightColor * g_vMaterialDiffuse * saturate( dot( vLight, vNormalWorld ) );\r\n"
		"	if( bSpecular ) {\r\n"
		"		float3 vCamera = normalize(vPosWorld.xyz - g_vCameraPosition);\r\n"
		"		float3 vReflection = reflect( vLight, vNormalWorld );\r\n"
		"		float  fPhongValue = saturate( dot( vReflection, vCamera ) );\r\n"
		"		vColorOut.rgb += g_vMaterialSpecular * pow(fPhongValue, g_nMaterialShininess);\r\n"
		"	}\r\n"
		"	vColorOut.a = g_fMaterialAlpha;\r\n"
		"	vColorOut *= vColorIn;\r\n"
		"}\r\n"
		"void Lighting( float4 vPos: POSITION0,\r\n"
		"	float2 vTexCoord: TEXCOORD0,\r\n"
		"	float4 vColorIn: COLOR0,\r\n"
		"	out float4 vColorOut: COLOR0,\r\n"
		"	uniform bool bTexture )\r\n"
		"{\r\n"
		"	vColorOut = vColorIn;\r\n"
		"	if( bTexture )\r\n"
		"		vColorOut *= tex2D( DiffuseSampler, vTexCoord );\r\n"
		"}\r\n"
		"technique Specular\r\n"
		"{\r\n"
		"	pass P0\r\n"
		"	{\r\n"
		"		VertexShader = compile vs_2_0 Projection(true);\r\n"
		"		PixelShader = compile ps_2_0 Lighting(false);\r\n"
		"	}\r\n"
		"}\r\n"
		"technique Wireframe\r\n"
		"{\r\n"
		"	pass P0\r\n"
		"	{\r\n"
		"		VertexShader = compile vs_2_0 Basic(1.000f);\r\n"
		"		PixelShader = compile ps_2_0 Lighting(false);\r\n"
		"	}\r\n"
		"	pass P1\r\n"
		"	{\r\n"
		"		VertexShader = compile vs_2_0 Basic(1.002f);\r\n"
		"		PixelShader = compile ps_2_0 Lighting(false);\r\n"
		"	}\r\n"
		"	pass P2\r\n"
		"	{\r\n"
		"		VertexShader = compile vs_2_0 Basic(1.004f);\r\n"
		"		PixelShader = compile ps_2_0 Lighting(false);\r\n"
		"	}\r\n"
		"}\r\n"
		"technique NoSpecular\r\n"
		"{\r\n"
		"	pass P0\r\n"
		"	{\r\n"
		"		VertexShader = compile vs_2_0 Projection(false);\r\n"
		"		PixelShader = compile ps_2_0 Lighting(false);\r\n"
		"	}\r\n"
		"}\r\n"
		"technique TexturedSpecular\r\n"
		"{\r\n"
		"	pass P0\r\n"
		"	{\r\n"
		"		VertexShader = compile vs_2_0 Projection(true);\r\n"
		"		PixelShader = compile ps_2_0 Lighting(true);\r\n"
		"	}\r\n"
		"}\r\n"
		"technique TexturedNoSpecular\r\n"
		"{\r\n"
		"	pass P0\r\n"
		"	{\r\n"
		"		VertexShader = compile vs_2_0 Projection(false);\r\n"
		"		PixelShader = compile ps_2_0 Lighting(true);\r\n"
		"	}\r\n"
		"}\r\n";

	UINT dwBufferSize = ( UINT )strlen( g_strBuffer ) + 1;

	try
	{
		if (D3DXCreateEffect(pDevice, g_strBuffer, dwBufferSize, NULL, NULL, D3DXFX_NOT_CLONEABLE, NULL, &m_pEffect, NULL) == D3D_OK)
		{
			if (m_pEffect) {
				m_hAmbient = m_pEffect->GetParameterBySemantic(0, "Ambient");
				m_hDiffuse = m_pEffect->GetParameterBySemantic(0, "Diffuse");
				m_hSpecular = m_pEffect->GetParameterBySemantic(0, "Specular");
				m_hOpacity = m_pEffect->GetParameterBySemantic(0, "Opacity");
				m_hSpecularPower = m_pEffect->GetParameterBySemantic(0, "SpecularPower");
				m_hLightColor = m_pEffect->GetParameterBySemantic(0, "LightColor");
				m_hLightPosition = m_pEffect->GetParameterBySemantic(0, "LightPosition");
				m_hCameraPosition = m_pEffect->GetParameterBySemantic(0, "CameraPosition");
				m_hTexture = m_pEffect->GetParameterBySemantic(0, "Texture");
				m_hTime = m_pEffect->GetParameterBySemantic(0, "Time");
				m_hWorld = m_pEffect->GetParameterBySemantic(0, "World");
				m_hWorldViewProjection = m_pEffect->GetParameterBySemantic(0, "WorldViewProjection");
				m_hWireframeColor = m_pEffect->GetParameterBySemantic(0, "WireframeColor");
				m_hTransform = m_pEffect->GetParameterBySemantic(0, "Transform");
			}
		}
	}
	catch ( ... )
	{
		_ERROR("%s - Fatal error creating D3D Effect.", __FUNCTION__);
		m_pEffect = nullptr;
	}
}

#endif