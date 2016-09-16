#version 400
uniform dvec2 topright;
uniform dvec2 bottomleft;
uniform vec2 resolution;

#define ITERATIONS 150
#define PI 3.14159

vec4 hsvToRgb(float H, float V)
{
	float H1 = H/0.16667;
	float R, G, B;
	float X = V * (1 - abs(mod(H1, 2) - 1));
	if ((H1 >= 0) && (H1 < 1))
	{
		R = V;
		G = X;
		B = 0;
	}
	else if ((H1 >= 1) && (H1 < 2))
	{
		R = X;
		G = V;
		B = 0;
	}
	else if ((H1 >= 2) && (H1 < 3))
	{
		R = 0;
		G = V;
		B = X;
	}
	else if ((H1 >= 3) && (H1 < 4))
	{
		R = 0;
		G = X;
		B = V;
	}
	else if ((H1 >= 4) && (H1 < 5))
	{
		R = X;
		G = 0;
		B = V;
	}
	else if ((H1 >= 5) && (H1 < 6))
	{
		R = V;
		G = 0;
		B = X;
	}
	return vec4(R, G, B, 1.0);
}

void main()
{
	dvec2 range = (topright - bottomleft);
	double x = 0.0;
	double y = 0.0;
	float shade;
	dvec2 p0 = dvec2(gl_FragCoord.x - 0.5 , gl_FragCoord.y - 0.5);
	p0 = p0 / resolution;
	double temp;
	p0 = dvec2(fma(p0, range, bottomleft));

	int i;
	for (i = 0; i < ITERATIONS; i++)
	{
		temp = x*x - y*y;
		y = 2 * x * y;
		x = temp;
		x += p0.x;
		y += p0.y;
		if (x*x + y*y >= 40)
		{
			break;
		}
	}
	shade = i / 150.0;
	gl_FragColor = hsvToRgb(shade, shade);
}