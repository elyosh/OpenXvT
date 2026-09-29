#include "xvt/math/trig2.h"

#include <math.h>

// GLOBAL: XVT 0x51815C
const float g_q16AngleToRadiansScale = 0.000095873722f;

// GLOBAL: XVT 0x518160
const float g_trigQ15OutputScale = 32767.0f;

// GLOBAL: XVT 0x524520
uint16_t g_sinTable[513] = {
	0,     402,   804,   1206,  1608,  2010,  2412,  2814,  3216,  3617,  4019,  4420,  4821,  5222,  5623,
	6023,  6424,  6824,  7224,  7623,  8022,  8421,  8820,  9218,  9616,  10014, 10411, 10808, 11204, 11600,
	11996, 12391, 12785, 13180, 13573, 13966, 14359, 14751, 15143, 15534, 15924, 16314, 16703, 17091, 17479,
	17867, 18253, 18639, 19024, 19409, 19792, 20175, 20557, 20939, 21320, 21699, 22078, 22457, 22834, 23210,
	23586, 23961, 24335, 24708, 25080, 25451, 25821, 26190, 26558, 26925, 27291, 27656, 28020, 28383, 28745,
	29106, 29466, 29824, 30182, 30538, 30893, 31248, 31600, 31952, 32303, 32652, 33000, 33347, 33692, 34037,
	34380, 34721, 35062, 35401, 35738, 36075, 36410, 36744, 37076, 37407, 37736, 38064, 38391, 38716, 39040,
	39362, 39683, 40002, 40320, 40636, 40951, 41264, 41576, 41886, 42194, 42501, 42806, 43110, 43412, 43713,
	44011, 44308, 44604, 44898, 45190, 45480, 45769, 46056, 46341, 46624, 46906, 47186, 47464, 47741, 48015,
	48288, 48559, 48828, 49095, 49361, 49624, 49886, 50146, 50404, 50660, 50914, 51166, 51417, 51665, 51911,
	52156, 52398, 52639, 52878, 53114, 53349, 53581, 53812, 54040, 54267, 54491, 54714, 54934, 55152, 55368,
	55582, 55794, 56004, 56212, 56418, 56621, 56823, 57022, 57219, 57414, 57607, 57798, 57986, 58172, 58356,
	58538, 58718, 58896, 59071, 59244, 59415, 59583, 59750, 59914, 60075, 60235, 60392, 60547, 60700, 60851,
	60999, 61145, 61288, 61429, 61568, 61705, 61839, 61971, 62101, 62228, 62353, 62476, 62596, 62714, 62830,
	62943, 63054, 63162, 63268, 63372, 63473, 63572, 63668, 63763, 63854, 63944, 64031, 64115, 64197, 64277,
	64354, 64429, 64501, 64571, 64639, 64704, 64766, 64827, 64884, 64940, 64993, 65043, 65091, 65137, 65180,
	65220, 65259, 65294, 65328, 65358, 65387, 65413, 65436, 65457, 65476, 65492, 65505, 65516, 65525, 65531,
	65533, 65534, 65533, 65531, 65525, 65516, 65505, 65492, 65476, 65457, 65436, 65413, 65387, 65358, 65328,
	65294, 65259, 65220, 65180, 65137, 65091, 65043, 64993, 64940, 64884, 64827, 64766, 64704, 64639, 64571,
	64501, 64429, 64354, 64277, 64197, 64115, 64031, 63944, 63854, 63763, 63668, 63572, 63473, 63372, 63268,
	63162, 63054, 62943, 62830, 62714, 62596, 62476, 62353, 62228, 62101, 61971, 61839, 61705, 61568, 61429,
	61288, 61145, 60999, 60851, 60700, 60547, 60392, 60235, 60075, 59914, 59750, 59583, 59415, 59244, 59071,
	58896, 58718, 58538, 58356, 58172, 57986, 57798, 57607, 57414, 57219, 57022, 56823, 56621, 56418, 56212,
	56004, 55794, 55582, 55368, 55152, 54934, 54714, 54491, 54267, 54040, 53812, 53581, 53349, 53114, 52878,
	52639, 52398, 52156, 51911, 51665, 51417, 51166, 50914, 50660, 50404, 50146, 49886, 49624, 49361, 49095,
	48828, 48559, 48288, 48015, 47741, 47464, 47186, 46906, 46624, 46341, 46056, 45769, 45480, 45190, 44898,
	44604, 44308, 44011, 43713, 43412, 43110, 42806, 42501, 42194, 41886, 41576, 41264, 40951, 40636, 40320,
	40002, 39683, 39362, 39040, 38716, 38391, 38064, 37736, 37407, 37076, 36744, 36410, 36075, 35738, 35401,
	35062, 34721, 34380, 34037, 33692, 33347, 33000, 32652, 32303, 31952, 31600, 31248, 30893, 30538, 30182,
	29824, 29466, 29106, 28745, 28383, 28020, 27656, 27291, 26925, 26558, 26190, 25821, 25451, 25080, 24708,
	24335, 23961, 23586, 23210, 22834, 22457, 22078, 21699, 21320, 20939, 20557, 20175, 19792, 19409, 19024,
	18639, 18253, 17867, 17479, 17091, 16703, 16314, 15924, 15534, 15143, 14751, 14359, 13966, 13573, 13180,
	12785, 12391, 11996, 11600, 11204, 10808, 10411, 10014, 9616,  9218,  8820,  8421,  8022,  7623,  7224,
	6824,  6424,  6023,  5623,  5222,  4821,  4420,  4019,  3617,  3216,  2814,  2412,  2010,  1608,  1206,
	804,   402,   0,
};

// GLOBAL: XVT 0x524C28
int16_t g_arctantable[260] = {
	0,    41,   81,   122,  163,  204,  244,  285,  326,  367,  407,  448,  489,  529,  570,  610,  651,
	692,  732,  773,  813,  854,  894,  935,  975,  1015, 1056, 1096, 1136, 1177, 1217, 1257, 1297, 1337,
	1377, 1417, 1457, 1497, 1537, 1577, 1617, 1656, 1696, 1736, 1775, 1815, 1854, 1894, 1933, 1973, 2012,
	2051, 2090, 2129, 2168, 2207, 2246, 2285, 2324, 2363, 2401, 2440, 2478, 2517, 2555, 2593, 2632, 2670,
	2708, 2746, 2784, 2822, 2860, 2897, 2935, 2973, 3010, 3047, 3085, 3122, 3159, 3196, 3233, 3270, 3307,
	3344, 3380, 3417, 3453, 3490, 3526, 3562, 3598, 3634, 3670, 3706, 3742, 3778, 3813, 3849, 3884, 3920,
	3955, 3990, 4025, 4060, 4095, 4129, 4164, 4199, 4233, 4267, 4302, 4336, 4370, 4404, 4438, 4471, 4505,
	4539, 4572, 4605, 4639, 4672, 4705, 4738, 4771, 4803, 4836, 4869, 4901, 4933, 4966, 4998, 5030, 5062,
	5093, 5125, 5157, 5188, 5220, 5251, 5282, 5313, 5344, 5375, 5406, 5437, 5467, 5498, 5528, 5558, 5589,
	5619, 5649, 5679, 5708, 5738, 5768, 5797, 5826, 5856, 5885, 5914, 5943, 5972, 6000, 6029, 6057, 6086,
	6114, 6142, 6171, 6199, 6226, 6254, 6282, 6310, 6337, 6365, 6392, 6419, 6446, 6473, 6500, 6527, 6554,
	6580, 6607, 6633, 6660, 6686, 6712, 6738, 6764, 6790, 6815, 6841, 6867, 6892, 6917, 6943, 6968, 6993,
	7018, 7043, 7067, 7092, 7117, 7141, 7166, 7190, 7214, 7238, 7262, 7286, 7310, 7334, 7358, 7381, 7405,
	7428, 7451, 7474, 7498, 7521, 7544, 7566, 7589, 7612, 7634, 7657, 7679, 7702, 7724, 7746, 7768, 7790,
	7812, 7834, 7856, 7877, 7899, 7920, 7942, 7963, 7984, 8005, 8026, 8047, 8068, 8089, 8110, 8130, 8151,
	8172, 8192, 8192, 0,    0,
};

// GLOBAL: XVT 0x524E30
uint16_t g_squarerootable[257] = {
	0,     0,     2,     4,     8,     12,    18,    24,    32,    40,    50,    60,    72,    84,    98,
	112,   128,   144,   162,   180,   200,   220,   242,   264,   287,   312,   337,   363,   391,   419,
	448,   479,   510,   542,   575,   610,   645,   681,   718,   756,   795,   835,   876,   918,   961,
	1005,  1050,  1095,  1142,  1190,  1238,  1288,  1338,  1390,  1442,  1495,  1550,  1605,  1661,  1718,
	1776,  1835,  1895,  1955,  2017,  2079,  2143,  2207,  2273,  2339,  2406,  2474,  2543,  2612,  2683,
	2755,  2827,  2900,  2974,  3049,  3125,  3202,  3280,  3358,  3438,  3518,  3599,  3681,  3764,  3847,
	3932,  4017,  4103,  4190,  4278,  4367,  4456,  4547,  4638,  4730,  4822,  4916,  5010,  5105,  5201,
	5298,  5396,  5494,  5593,  5693,  5794,  5895,  5997,  6100,  6204,  6309,  6414,  6520,  6627,  6734,
	6843,  6952,  7061,  7172,  7283,  7395,  7508,  7621,  7735,  7850,  7966,  8082,  8199,  8317,  8435,
	8554,  8674,  8794,  8915,  9037,  9160,  9283,  9407,  9531,  9656,  9782,  9909,  10036, 10164, 10292,
	10421, 10551, 10681, 10812, 10944, 11076, 11209, 11343, 11477, 11612, 11747, 11883, 12019, 12157, 12294,
	12433, 12572, 12711, 12852, 12992, 13134, 13276, 13418, 13561, 13705, 13849, 13994, 14139, 14285, 14431,
	14578, 14726, 14874, 15022, 15171, 15321, 15471, 15622, 15773, 15925, 16077, 16230, 16384, 16537, 16692,
	16847, 17002, 17158, 17314, 17471, 17629, 17786, 17945, 18104, 18263, 18423, 18583, 18744, 18905, 19066,
	19229, 19391, 19554, 19718, 19882, 20046, 20211, 20376, 20542, 20708, 20875, 21042, 21209, 21377, 21546,
	21714, 21884, 22053, 22223, 22394, 22565, 22736, 22908, 23080, 23252, 23425, 23599, 23772, 23946, 24121,
	24296, 24471, 24647, 24823, 24999, 25176, 25353, 25531, 25709, 25887, 26066, 26245, 26424, 26604, 26784,
	26964, 27146,
};

// GLOBAL: XVT 0x9A20B0
int trig2_xmovedist = 0;

// GLOBAL: XVT 0x9A8E14
int trig2_xoffset = 0;

// GLOBAL: XVT 0x9A8C00
int trig2_polardistance = 0;

// GLOBAL: XVT 0x9A8070
uint16_t trig2_xyangle = 0;

// GLOBAL: XVT 0x9CD26C
int trig2_ymovedist = 0;

// GLOBAL: XVT 0x9D12B0
uint16_t trig2_phi = 0;

// GLOBAL: XVT 0x9D6824
int trig2_rho = 0;

// GLOBAL: XVT 0x9E9648
int trig2_yoffset = 0;

// GLOBAL: XVT 0x9EC460
int16_t trig2_signswap = 0;

// GLOBAL: XVT 0x9ECC46
uint16_t trig2_signx = UINT16_MAX;

// GLOBAL: XVT 0x9ECC48
uint16_t trig2_signy = UINT16_MAX;

// GLOBAL: XVT 0x9ECC50
uint16_t trig2_signz = 0;

// GLOBAL: XVT 0x9FD438
int trig2_zmovedist = 0;

// GLOBAL: XVT 0xA00520
int trig2_zoffset = 0;

// GLOBAL: XVT 0xA00740
int trig2_divisorhilo = 0;

// GLOBAL: XVT 0xA07C5C
uint16_t trig2_theta = 0;

// GLOBAL: XVT 0xA080F0
uint16_t pitchQ16 = 0;

// GLOBAL: XVT 0xA080FE
int16_t trig2_angleplane = 0;

// FLAGS: /O2 /G5
// FUNCTION: XVT 0x46A3C0
int16_t trig2_getsignedsin(int16_t angleQ16) {
	return (int16_t)(sin(angleQ16 * g_q16AngleToRadiansScale) * g_trigQ15OutputScale);
}

// FUNCTION: XVT 0x46A3F0
int16_t trig2_calcsineofangle(int16_t angle) {
	uint16_t tableIndex;
	uint16_t base;
	uint16_t deltaMagnitude;
	int16_t signedDelta;
	uint32_t interpolation;

	tableIndex = ((uint16_t)angle >> 6) & 0x1FFu;
	base = g_sinTable[tableIndex];
	deltaMagnitude = (uint16_t)(g_sinTable[tableIndex + 1] - base);
	signedDelta = (int16_t)deltaMagnitude;
	if (signedDelta < 0) {
		deltaMagnitude = (uint16_t)-deltaMagnitude;
	}

	interpolation = ((uint32_t)(uint16_t)((uint16_t)angle << 10) * deltaMagnitude) >> 16;
	if (signedDelta < 0) {
		interpolation = 0u - interpolation;
	}

	return (int16_t)(base + interpolation);
}

// FUNCTION: XVT 0x46A450
int16_t trig2_w_arcsin(int16_t sinQ15) { return trig2_arcsin(sinQ15); }

// FUNCTION: XVT 0x46A460
int16_t trig2_w_arccos(int16_t cosQ15) { return trig2_arccos(cosQ15); }

// FUNCTION: XVT 0x46A470
int16_t trig2_arccos(int16_t cosQ15) {
	int16_t tableIndex;
	int16_t remainingSteps;
	uint16_t target;
	uint16_t divisor;
	uint16_t base;
	uint16_t span;
	uint16_t delta;
	uint16_t interpolation;
	uint16_t angle;

	tableIndex = 256;
	target = (uint16_t)cosQ15;
	if (cosQ15 < 0) {
		target = (uint16_t)-target;
	}
	target = (uint16_t)(target + target);
	remainingSteps = 256;
	do {
		if (g_sinTable[tableIndex] <= target) {
			--remainingSteps;
			base = g_sinTable[tableIndex - 1];
			delta = (uint16_t)(target - base);
			if (delta != 0) {
				span = (uint16_t)(g_sinTable[tableIndex - 2] - base);
				/* Keep the shifted 32-bit pattern signed for division. */
				interpolation = (uint16_t)((uint16_t)((int32_t)((uint32_t)delta << 16) / (int32_t)span) >> 8);
			} else {
				interpolation = 0;
			}
			angle = (int16_t)(-1 - remainingSteps);
			angle = (int16_t)((uint16_t)angle << 8);
			angle = (int16_t)(angle - interpolation);
			angle = (int16_t)((uint16_t)angle >> 2);
			if (cosQ15 < 0) {
				angle = (uint16_t)-angle;
				angle += 0x8000u;
			}
			return angle;
		}
		--remainingSteps;
		++tableIndex;
	} while (remainingSteps > 0);

	divisor = g_sinTable[tableIndex - 1];
	interpolation = (uint16_t)((int32_t)((uint32_t)target << 16) / (int32_t)divisor);
	interpolation >>= 8;
	interpolation = (uint16_t)-interpolation;
	if (interpolation == 0) {
		angle = 0x4000;
	} else {
		angle = (int16_t)(interpolation >> 2);
	}
	if (cosQ15 < 0) {
		angle = (uint16_t)-angle;
		angle += 0x8000u;
	}

	return angle;
}

// FUNCTION: XVT 0x46A550
int16_t trig2_arcsin(int16_t sinQ15) {
	int16_t tableIndex;
	int16_t remainingSteps;
	int tableOffset;
	uint16_t target;
	uint16_t span;
	uint16_t delta;
	uint16_t interpolation;
	int16_t angle;

#ifdef XVT_MODERN
	interpolation = 0;
#endif
	tableIndex = 0;
	target = (uint16_t)sinQ15;
	if (sinQ15 < 0)
		target = (uint16_t)-target;
	target = (uint16_t)(target + target);

	remainingSteps = 256;
	while (g_sinTable[tableIndex] < target) {
		--remainingSteps;
		++tableIndex;
		if (remainingSteps <= 0)
			break;
	}

	tableOffset = tableIndex;
	--remainingSteps;
	tableIndex = 0;
	span = 0;
	if (tableOffset >= 2) {
		tableIndex = (int16_t)g_sinTable[tableOffset - 2];
		span = (uint16_t)(g_sinTable[tableOffset - 1] - (uint16_t)tableIndex);
	}
	delta = (uint16_t)(target - (uint16_t)tableIndex);
	if (delta != 0) {
		interpolation = (uint16_t)(((int)(uint16_t)delta << 16) / (int)(uint16_t)span);
		interpolation >>= 8;
	}

	angle = (int16_t)(-1 - remainingSteps);
	angle = (int16_t)((uint16_t)angle << 8);
	angle = (int16_t)(angle + interpolation);
	angle = (int16_t)((uint16_t)angle >> 2);
	if (sinQ15 < 0)
		angle = (int16_t)-angle;

	return angle;
}

// FUNCTION: XVT 0x46A5F0
unsigned int trig2_sinewordmult(int16_t arg1, int16_t arg2) {
	uint16_t sine;
	uint16_t signDifference;
	uint32_t product;

	sine = (uint16_t)arg1 & 0x8000u;
	if (sine != 0) {
		arg1 = (int16_t)-arg1;
	}
	signDifference = sine ^ ((uint16_t)arg2 & 0x8000u);
	sine = g_sinTable[((uint16_t)arg2 >> 6) & 0x1FFu];
	product = (uint32_t)sine * (uint16_t)arg1 + 0x8000u;
	if (signDifference != 0) {
		product = 0u - product;
	}

	return product >> 16;
}

// FUNCTION: XVT 0x46A650
int trig2_sinedwordmult(int value, uint16_t angle) {
	int16_t sign;
	uint16_t tableValue;
	uint32_t lowProduct;

	sign = 0;
	if (value < 0) {
		sign = 0x8000u;
		value = (int)(0u - (uint32_t)value);
	}
	sign ^= (uint16_t)angle;
	tableValue = g_sinTable[(((uint16_t)angle >> 5) & 0x3FEu) >> 1];
	lowProduct = ((tableValue * (uint32_t)(uint16_t)value) + 0x8000u) >> 16;
	value = (int)((uint32_t)value >> 16);
	value = (int)((uint32_t)value * tableValue + lowProduct);
	if ((sign & 0x8000u) != 0) {
		return -value;
	}
	return value;
}

// FUNCTION: XVT 0x46A6D0
int16_t trig2_getsignedcos(int16_t angleQ16) {
	return (int16_t)(cos(angleQ16 * g_q16AngleToRadiansScale) * g_trigQ15OutputScale);
}

// FUNCTION: XVT 0x46A700
unsigned int trig2_cosinewordmult(uint16_t value, int16_t angle) {
	uint16_t sine;
	uint16_t signDifference;
	uint32_t product;

	sine = value & 0x8000u;
	if (sine != 0) {
		value = (uint16_t)(0u - value);
	}
	angle = (int16_t)(angle + 0x4000);
	signDifference = sine ^ ((uint16_t)angle & 0x8000u);
	sine = g_sinTable[((uint16_t)angle >> 6) & 0x1FFu];
	product = (uint32_t)sine * value + 0x8000u;
	if (signDifference != 0) {
		product = 0u - product;
	}

	return product >> 16;
}

// FUNCTION: XVT 0x46A760
int trig2_cosinedwordmult(int value, uint16_t angle) {
	uint16_t sign;
	uint16_t shiftedAngle;
	uint16_t tableValue;
	uint32_t lowProduct;
	uint32_t magnitude;
	int result;

	sign = 0;
	magnitude = (uint32_t)value;
	if (value < 0) {
		sign = (int16_t)0x8000u;
		magnitude = 0u - magnitude;
	}
	shiftedAngle = (uint16_t)angle;
	shiftedAngle += 0x4000u;
	sign ^= (int16_t)shiftedAngle;
	shiftedAngle >>= 5;
	sign &= (int16_t)0x8000u;
	shiftedAngle &= 0x3FEu;
	shiftedAngle >>= 1;
	tableValue = g_sinTable[shiftedAngle];
	lowProduct = ((tableValue * (uint32_t)(uint16_t)magnitude) + 0x8000u) >> 16;
	magnitude >>= 16;
	magnitude *= tableValue;
	result = (int)(magnitude + lowProduct);
	if (sign != 0) {
		return -result;
	}
	return result;
}

// FUNCTION: XVT 0x46A7C0
void trig2_UpdateCartesianOffsets(void) {
	trig2_zoffset = trig2_sinedwordmult(trig2_rho, trig2_phi);
	trig2_xoffset = trig2_cosinedwordmult(trig2_zoffset, trig2_theta);
	trig2_yoffset = trig2_sinedwordmult(trig2_zoffset, trig2_theta);
	trig2_zoffset = trig2_cosinedwordmult(trig2_rho, trig2_phi);
}

// FUNCTION: XVT 0x46A870
void trig2_movexyz(uint16_t distance, int16_t yaw, uint16_t pitch) {
	trig2_theta = (uint16_t)(0x4000 - yaw);
	trig2_rho = distance;
	trig2_phi = pitch;
	trig2_UpdateCartesianOffsets();
	trig2_xmovedist = trig2_xoffset;
	trig2_ymovedist = trig2_yoffset;
	trig2_zmovedist = trig2_zoffset;
}

// FUNCTION: XVT 0x46A8D0
void trig2_ctop(int dx, int dy, int dz) {
	int magnitudeX;
	int magnitudeY;
	int magnitudeZ;
	int16_t angle;

	magnitudeX = dx;
	if (magnitudeX < 0) {
		magnitudeX = -magnitudeX;
		trig2_signx = 1;
	} else {
		trig2_signx = 0;
	}
	magnitudeY = dy;
	trig2_xoffset = magnitudeX;
	if (magnitudeY < 0) {
		magnitudeY = -magnitudeY;
		trig2_signy = 1;
	} else {
		trig2_signy = 0;
	}
	magnitudeZ = dz;
	trig2_yoffset = magnitudeY;
	if (magnitudeZ < 0) {
		magnitudeZ = -magnitudeZ;
		trig2_signz = 1;
	} else {
		trig2_signz = 0;
	}
	trig2_zoffset = magnitudeZ;

	trig2_calcangleplanedistance(magnitudeX, magnitudeY);
	angle = trig2_angleplane;
	trig2_xyangle = angle;
	if (trig2_signy != 0) {
		angle = (int16_t)-trig2_angleplane;
	}
	if (trig2_signx != 0) {
		angle = (int16_t)(0x8000u - (uint16_t)angle);
	}
	trig2_xyangle = angle;
	trig2_xyangle = (int16_t)(0x4000 - angle);

	trig2_calcangleplanedistance(trig2_polardistance, trig2_zoffset);
	angle = trig2_angleplane;
	pitchQ16 = angle;
	if (trig2_signz != 0) {
		angle = (int16_t)-trig2_angleplane;
	}
	pitchQ16 = angle;
	pitchQ16 = (int16_t)(0x4000 - angle);
}

// FUNCTION: XVT 0x46A9E0
void trig2_ctop2dim(int dx, int dy) {
	int magnitudeX;
	int magnitudeY;

	magnitudeX = dx;
	if (magnitudeX < 0) {
		magnitudeX = -magnitudeX;
		trig2_signx = 1;
	} else {
		trig2_signx = 0;
	}
	magnitudeY = dy;
	if (magnitudeY < 0) {
		magnitudeY = -magnitudeY;
		trig2_signy = 1;
	} else {
		trig2_signy = 0;
	}
	trig2_calcangleplanedistance(magnitudeX, magnitudeY);
	trig2_xyangle = (uint16_t)trig2_angleplane;
	if (trig2_signy != 0) {
		trig2_xyangle = (uint16_t)-trig2_angleplane;
	}
	if (trig2_signx != 0) {
		trig2_xyangle = (uint16_t)(0x8000u - trig2_xyangle);
	}
	trig2_xyangle = (uint16_t)(0x4000u - trig2_xyangle);
}

// FUNCTION: XVT 0x46AA70
int trig2_calcangleplanedistance(int magnitudeA, int magnitudeB) {
	int16_t outRatio;
	int16_t outAngle;
	uint32_t scale;
	uint32_t divisor;
	uint32_t highProduct;
	uint32_t lowProduct;
	uint16_t roundedLowProduct;

	trig2_calcarctan_core(magnitudeA, magnitudeB, &outRatio, &outAngle);
	trig2_angleplane = outRatio;
	scale = g_squarerootable[(uint16_t)outAngle];
	divisor = (uint32_t)trig2_divisorhilo;
	highProduct = (divisor >> 16) * scale;
	lowProduct = (divisor & 0xFFFFu) * scale;
	lowProduct += 0x8000u;
	lowProduct >>= 16;
	roundedLowProduct = (uint16_t)lowProduct;
	trig2_polardistance = (int)(divisor + highProduct + roundedLowProduct);
	return trig2_polardistance;
}

// FUNCTION: XVT 0x46AAF0
int16_t trig2_calcarctan_core(int a, int b, int16_t* outRatio, int16_t* outAngle) {
	uint32_t numerator;
	uint32_t fraction;
	uint32_t divisor;
	uint16_t tableDelta;
	uint32_t interpolation;
	uint32_t swap;
	int16_t result;

	numerator = (uint32_t)b;
	divisor = (uint32_t)a;
	fraction = 0;
	trig2_signswap = 0;
	if (numerator == divisor) {
		numerator = 256;
		trig2_divisorhilo = (int)divisor;
	} else {
		if ((int32_t)numerator >= (int32_t)divisor) {
			trig2_signswap = 1;
			swap = numerator;
			numerator = divisor;
			divisor = swap;
		}
		trig2_divisorhilo = (int)divisor;
		if (divisor != 0) {
			if ((divisor & 0xFF000000u) == 0) {
				divisor <<= 8;
				numerator <<= 8;
				if ((divisor & 0xFF000000u) == 0) {
					divisor <<= 8;
					numerator <<= 8;
				}
			}
			if (numerator == divisor) {
				fraction = numerator >> 16;
				numerator = 256;
			} else {
				divisor >>= 16;
				numerator = (uint16_t)(numerator / divisor);
				fraction = (numerator & 0xFFu) << 8;
				numerator = (uint32_t)((int32_t)numerator >> 8);
			}
		} else {
			fraction = numerator >> 16;
			numerator = 0;
		}
	}

	*outAngle = (int16_t)numerator;
	*outRatio = g_arctantable[(uint16_t)numerator + 1];
	tableDelta = (uint16_t)(*outRatio - g_arctantable[(uint16_t)*outAngle]);
	*outRatio = (int16_t)tableDelta;
	interpolation = ((fraction & 0xFF00u) * tableDelta) >> 16;
	*outRatio = (int16_t)interpolation;
	result = (int16_t)(interpolation + g_arctantable[(uint16_t)*outAngle]);
	*outRatio = result;
	if (trig2_signswap != 0) {
		result = (int16_t)-result;
		*outRatio = result;
		result = (int16_t)(result + 0x4000);
		*outRatio = result;
	}
	return result;
}

// FUNCTION: XVT 0x46ABF0
int16_t trig2_arctan(int y, int x) {
	int magnitudeY;
	int magnitudeX;
	int16_t outAngle;
	int16_t outRatio;

	magnitudeY = y;
	if (magnitudeY < 0) {
		magnitudeY = -magnitudeY;
		trig2_signy = 1;
	} else {
		trig2_signy = 0;
	}
	magnitudeX = x;
	if (magnitudeX < 0) {
		magnitudeX = -magnitudeX;
		trig2_signx = 1;
	} else {
		trig2_signx = 0;
	}
	trig2_calcarctan_core(magnitudeX, magnitudeY, &outRatio, &outAngle);
	if (trig2_signy != 0) {
		outRatio = (int16_t)-outRatio;
	}
	if (trig2_signx != 0) {
		outRatio = (int16_t)(0x8000u - (uint16_t)outRatio);
	}
	return outRatio;
}
