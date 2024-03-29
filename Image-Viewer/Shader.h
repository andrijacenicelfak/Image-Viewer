#pragma once
#include <string>
#include <glad/glad.h>
#include "ImageShaderModification.h"
#include "ImageManagment.h"
class Shader
{
public:
	Shader(){
	}
	~Shader(){
		deleteShader();
	}

	void loadShader(char* vertexSource = nullptr, char* fragmentSource = nullptr);

	int fragShaderID = -1;
	int vertShaderID = -1;
	int shaderProgramID = -1;

	void drawImageWithModification(int texID, Image* mod);

	void activate();
	void deactivate();

	void deleteShader();

	void U1f(const char* uName, float uValue);
	void U2f(const char* uName, float uValue, float uValue2);
	void U3f(const char* uName, float uValue, float uValue2, float uValue3);
	void U1i(const char* uName, int uValue);
	void U1fv(const char* uName, int uValue, float uValue2[]);
private:
	unsigned int posData = 0, colorData = 0, uvData = 0;

	std::string vertexShaderSource = R"END(
	#version 330
	attribute vec3 inPosition;
	attribute vec4 inColor;
	attribute vec2 inUv;

	varying vec2 outUv;
	varying vec4 outColor;
	varying vec3 outPosition;
	void main(){
		outUv = inUv;
		outColor = inColor;
		outPosition = inPosition;
		gl_Position = vec4(inPosition, 1.0);
	}
	)END";
	
	std::string fragmentShaderSource = R"END(
	#version 330
	varying vec2 outUv;
	varying vec4 outColor;
	varying vec3 outPosition;

	uniform sampler2D sampler;
	uniform vec3 hsv;

	vec3 rgb2hsv(vec3 c)
	{
		vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
		vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
		vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

		float d = q.x - min(q.w, q.y);
		float e = 1.0e-10;
		return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
	}

	vec3 hsv2rgb(vec3 c)
	{
		vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
		vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
		return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
	}

	vec3 changeHsv(vec3 col, vec3 hsv){
		vec3 col_hsv = rgb2hsv(col);
		col_hsv.x = mod(col_hsv.x + hsv.x, 1.0);
		col_hsv.y = min(max(col_hsv.y * hsv.y, 0.0), 1.0);
		col_hsv.z = min(max(col_hsv.z * hsv.z, 0.0), 1.0);
		return hsv2rgb(col_hsv);
	}
	void main()
	{
		vec4 color = texture2D(sampler, outUv).rgba;
		float a = color.a;
		float c = int((outPosition.x + 1.0) * 55.0) % 2 != int((outPosition.y + 1.0) * 55.0) % 2 ? 0.65 : 0.9;
		gl_FragColor = vec4(vec3(c, c, c) * (1.0 - a) + changeHsv(color.rgb, hsv) * a, 1.0);
	}
	)END";
};

