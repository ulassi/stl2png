#version 400
in vec3 color;
in vec3 normal;
in vec3 vert2eye;


vec3 g_light0_dir = vec3(1.f, 1.f, 0.5f);
vec3 g_light0_col = vec3(1.f, 1.f, 0.95f);
vec3 g_light1_dir = vec3(-1.f, 0.1f, -0.5f);
vec3 g_light1_col = vec3(0.1f, 0.1f, 0.2f);
vec3 g_light2_dir = vec3(-0.1f, -0.5f, -0.5f);
vec3 g_light2_col = vec3(0.1f, 0.1f, 0.0f);


// material properties
float kd = 0.1;
vec3 ior = vec3(1.0);
float roughness = 0.2;
float metallic = 0.9;

// diffuse light contribution
vec3 diffuse(vec3 n) {
	vec3 d = vec3(0.f);
	d += clamp(dot(n,normalize(g_light0_dir)), 0.0, 1.0)*g_light0_col;
	d += clamp(dot(n,normalize(g_light1_dir)), 0.0, 1.0)*g_light1_col;
	d += clamp(dot(n,normalize(g_light2_dir)), 0.0, 1.0)*g_light2_col;
	return d;
}

#define PI 3.14159

float cook_torrance_chi(float v)
{
    return v > 0 ? 1 : 0;
}

// schlick fresnel
vec3 cook_torrance_f(vec3 f0, vec3 n, vec3 v) {
	return f0 + (1.0-f0)*pow(1.0-max(0,dot(n,v)), 5.0);
}

// microfacet distribution
float cook_torrance_d(vec3 n, vec3 h, float alpha) {
    float NoH = dot(n,h);
    float alpha2 = alpha * alpha;
    float NoH2 = NoH * NoH;
    float den = NoH2 * alpha2 + (1 - NoH2);
    return (cook_torrance_chi(NoH) * alpha2) / ( PI * den * den );
}

// microfacet geometry
float cook_torrance_g(vec3 n, vec3 v, vec3 h, float alpha) {
	float VoH2 = max(0,dot(v,h));
    float chi = cook_torrance_chi( VoH2 / max(0.0,dot(v,n)));
    VoH2 = VoH2 * VoH2;
    float tan2 = ( 1 - VoH2 ) / VoH2;
    return (chi * 2) / ( 1 + sqrt( 1 + alpha * alpha * tan2 ) );
}

vec3 cook_torrance_specular(
	vec3 normal,
	vec3 lightDir,
	vec3 viewDir,
	vec3 lightColor,
	vec3 material )
{
	float NdotL = max(0, dot(normal, lightDir));
	vec3 spec_response = vec3(0.0);
	if (NdotL > 0) 
	{
		vec3 H = normalize(lightDir + viewDir);
		float NdotH = max(0, dot(normal, H));
		float NdotV = max(0, dot(normal, viewDir));
		float VdotH = max(0, dot(lightDir, H));

		vec3 F0 = abs((1.0 - ior)/(1.0+ior));
		F0 *= F0;
		F0 = mix(F0, material, metallic);
		vec3 F = cook_torrance_f(F0, normal, viewDir);
		float D = cook_torrance_d(normal, H, roughness);
		float G = cook_torrance_g(normal, viewDir, H, roughness);
		
		spec_response = (D * F * G) / (PI * NdotL * NdotV);
	}
	return NdotL * lightColor * spec_response;
}

vec3 specular(vec3 n, vec3 material) {
	vec3 s = vec3(0.f);
	vec3 eye = normalize(vert2eye);
	s += cook_torrance_specular(n, g_light0_dir, eye, g_light0_col,material);
	s += cook_torrance_specular(n, g_light1_dir, eye, g_light1_col,material);
	s += cook_torrance_specular(n, g_light2_dir, eye, g_light2_col,material);
	return s;
}

void main() {
	vec3 col = vec3(0);
	col = color*((1.0 - kd)*specular(normal,color)+kd*diffuse(normal));
	gl_FragColor = vec4(col,1.0);
}