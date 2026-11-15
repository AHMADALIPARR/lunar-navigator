#version 330 core
/* kernel_skydome.frag — sky-dome kernel.
 *
 * Renders one site's whole sky as a fisheye dome, driven entirely
 * by real JPL Horizons samples: uSun / uEarth hold the 6-hourly
 * azimuth/elevation pairs (degrees), uCount of them, uTime is the
 * sample index of the bright "now" discs (mid-window, sample 28).
 *
 * Projection: the center is the zenith (+90 deg), the edge is
 * -10 deg, so the bold horizon ring sits at 0.9 of the radius and
 * the below-horizon part of a path stays visible instead of being
 * clipped. North is up, azimuth grows clockwise (east right).
 *
 * What is drawn, and what is not invented:
 *   - gold Sun path, solid above the horizon, dashed grey below it
 *     (same split as the SVG plots this replaces);
 *   - blue Earth dots, one per sample, joined by a faint line;
 *   - the bright Sun and Earth discs at the "now" sample.
 * Discs are drawn larger than their true angular size (Sun 0.27 deg,
 * Earth 0.95 deg) so they read at 640 px; the page says so.
 * No stars, no terrain, no invented bodies: below the horizon is
 * flat dark ground, nothing more.
 */
uniform vec2 uResolution;
uniform float uTime;
uniform vec2 uSun[64];
uniform vec2 uEarth[64];
uniform int uCount;
out vec4 outColor;

const float PI = 3.141592653589793;
const float D2R = PI / 180.0;

vec3 skyDir(float azDeg, float elDeg)
{
    float az = azDeg * D2R, el = elDeg * D2R;
    return vec3(cos(el) * sin(az), sin(el), cos(el) * cos(az));
}

/* Angular distance (radians) from unit dir d to segment a->b. */
float segDist(vec3 d, vec3 a, vec3 b)
{
    vec3 ba = b - a;
    float t = clamp(dot(d - a, ba) / dot(ba, ba), 0.0, 1.0);
    vec3 p = a + t * ba;
    return acos(clamp(dot(d, normalize(p)), -1.0, 1.0));
}

void main()
{
    vec2 ndc = (gl_FragCoord.xy - 0.5 * uResolution) / (0.5 * uResolution.y);
    float r = length(ndc);
    /* Outside the dome: the page background, so the image blends in. */
    if (r > 1.0) { outColor = vec4(0.027, 0.043, 0.075, 1.0); return; }
    float el = 90.0 - r * 100.0;             /* +90 center .. -10 edge */
    float az = degrees(atan(ndc.x, ndc.y)); /* 0 = North (up), +clockwise */
    vec3 d = skyDir(az, el);

    /* Sky above the horizon, dark ground below it. */
    vec3 col;
    if (el >= 0.0) {
        float g = clamp(el / 60.0, 0.0, 1.0);
        col = mix(vec3(0.10, 0.16, 0.30), vec3(0.015, 0.03, 0.09), g);
    } else {
        float g = clamp(-el / 10.0, 0.0, 1.0);
        col = mix(vec3(0.10, 0.09, 0.08), vec3(0.03, 0.028, 0.025), g);
    }

    /* Grid: elevation rings at +30/+60, spokes every 30 deg of azimuth. */
    float ring = 0.0;
    ring += 1.0 - smoothstep(0.0, 0.004, abs(el - 30.0) * D2R);
    ring += 1.0 - smoothstep(0.0, 0.004, abs(el - 60.0) * D2R);
    float azm = mod(az, 30.0);
    float dAz = min(azm, 30.0 - azm) * D2R * cos(el * D2R);
    float spoke = 1.0 - smoothstep(0.0, 0.005, dAz);
    spoke *= smoothstep(0.06, 0.15, r); /* azimuth is meaningless at the pole */
    col += vec3(0.10, 0.12, 0.16) * ring * 0.5;
    col += vec3(0.08, 0.10, 0.14) * spoke * 0.4;

    /* Sun path: nearest segment, and whether that stretch is up or down. */
    float best = 1e3;
    int bestSeg = 0;
    float bestT = 0.0;
    for (int i = 0; i < 63; i++) {
        if (i >= uCount - 1) break;
        vec3 a = skyDir(uSun[i].x, uSun[i].y);
        vec3 b = skyDir(uSun[i + 1].x, uSun[i + 1].y);
        float dist = segDist(d, a, b);
        if (dist < best) {
            best = dist;
            bestSeg = i;
            vec3 ba = b - a;
            bestT = clamp(dot(d - a, ba) / dot(ba, ba), 0.0, 1.0);
        }
    }
    float midEl = mix(uSun[bestSeg].y, uSun[bestSeg + 1].y, bestT);
    float core = 1.0 - smoothstep(0.0, 0.010, best);
    float glow = 1.0 - smoothstep(0.010, 0.05, best);
    if (midEl >= 0.0) {
        col = mix(col, vec3(1.0, 0.72, 0.20), core * 0.95);
        col += vec3(1.0, 0.62, 0.10) * glow * 0.35;
    } else {
        /* Dashed grey while below the horizon: one sample on, one off. */
        float dash = step(0.5, fract((float(bestSeg) + bestT) * 0.5));
        col = mix(col, vec3(0.55, 0.58, 0.62), core * 0.9 * dash);
        col += vec3(0.35, 0.37, 0.40) * glow * 0.25 * dash;
    }

    /* Earth: faint line through the samples, one dot per sample. */
    float eBest = 1e3;
    float eDot = 1e3;
    for (int i = 0; i < 63; i++) {
        if (i >= uCount - 1) break;
        vec3 a = skyDir(uEarth[i].x, uEarth[i].y);
        vec3 b = skyDir(uEarth[i + 1].x, uEarth[i + 1].y);
        eBest = min(eBest, segDist(d, a, b));
        eDot = min(eDot, acos(clamp(dot(d, a), -1.0, 1.0)));
    }
    if (uCount > 0) {
        vec3 last = skyDir(uEarth[uCount - 1].x, uEarth[uCount - 1].y);
        eDot = min(eDot, acos(clamp(dot(d, last), -1.0, 1.0)));
    }
    float eCore = 1.0 - smoothstep(0.0, 0.006, eBest);
    float eDotM = 1.0 - smoothstep(0.004, 0.012, eDot);
    col = mix(col, vec3(0.22, 0.74, 0.97), max(eCore * 0.7, eDotM * 0.95));

    /* Bold horizon ring, over the paths. */
    float hor = 1.0 - smoothstep(0.0, 0.006, abs(el) * D2R);
    col = mix(col, vec3(0.91, 0.91, 0.91), hor * 0.85);

    /* "Now" discs at the mid-window sample. */
    int now = clamp(int(uTime + 0.5), 0, uCount - 1);
    vec3 sunNow = skyDir(uSun[now].x, uSun[now].y);
    float ds = acos(clamp(dot(d, sunNow), -1.0, 1.0));
    float sunDisc = 1.0 - smoothstep(0.020, 0.030, ds);
    float sunHalo = 1.0 - smoothstep(0.030, 0.14, ds);
    vec3 sunCol = (uSun[now].y >= 0.0) ? vec3(1.0, 0.85, 0.45)
                                        : vec3(0.60, 0.62, 0.66);
    col = mix(col, sunCol, sunDisc * 0.98);
    col += vec3(1.0, 0.70, 0.25) * sunHalo * 0.30;

    vec3 earthNow = skyDir(uEarth[now].x, uEarth[now].y);
    float de = acos(clamp(dot(d, earthNow), -1.0, 1.0));
    float earthDisc = 1.0 - smoothstep(0.030, 0.045, de);
    float earthHalo = 1.0 - smoothstep(0.045, 0.16, de);
    col = mix(col, vec3(0.65, 0.85, 1.0), earthDisc * 0.98);
    col += vec3(0.25, 0.55, 0.95) * earthHalo * 0.25;

    outColor = vec4(col, 1.0);
}
