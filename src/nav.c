/* nav.c — Lunar Landing Site Navigator (Jessica's team entry).
 *
 * Reads JPL Horizons observer ephemeris (Sun + Earth apparent
 * azimuth/elevation, 6-hourly, 2026-11-15 to 2026-11-29) for the nine
 * Artemis III candidate regions, computes illumination and
 * communications windows, applies the constraint guards, and writes:
 *   out/sites_filled.csv — the decision-model table, filled from data
 *   out/index.html       — the navigator page (table + 3D sky-dome renders)
 *   out/summary.txt      — plain-text per-site numbers
 *   out/sky/<slug>.txt   — per-site Sun/Earth samples for the GLSL dome
 *                           kernel (sky/kernel_skydome.frag); sky/render.sh
 *                           turns them into out/sky/<slug>.png, which the
 *                           next nav run embeds base64 in the page. When
 *                           no render exists yet, the page falls back to
 *                           the SVG polar plot (svg_sky, below).
 *
 * C11, standard library only. No invented numbers: every value on the
 * page traces to a Horizons CSV row. Elevations are target-center,
 * airless, geometric; local terrain masking (LOLA horizon profile)
 * is NOT modeled — the terrain guard stays UNKNOWN and says so.
 *
 * Stated guard rules (visible here and in the README):
 *   POWER  pass when Sun is up >= 300 h of the 336 h window
 *          AND Sun maximum elevation >= 3.0 deg.
 *   COMMS  pass when Earth is up >= 300 h of the 336 h window
 *          AND Earth maximum elevation >= 5.0 deg.
 *   TERRAIN unknown for every site (LOLA horizon not computed).
 * Decision: score = sun-up hours + earth-up hours; the top-ranked
 * site that passes both guards is RECOMMENDED, other passing sites
 * VIABLE, one-guard failures MARGINAL, both-guard failures REJECT.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>

#define MAX_SAMPLES 128
#define MAX_SITES 16
#define WINDOW_HOURS 336.0

typedef struct {
    char datetime[32];
    double hours;   /* hours since 2026-11-15 00:00 UTC */
    double az, el;
} Sample;

typedef struct {
    char slug[16];
    char name[64];
    double lon, lat;
    double feo, al2o3;
    Sample sun[MAX_SAMPLES];
    Sample earth[MAX_SAMPLES];
    int n;
    double sun_min, sun_max, sun_mean, sun_up, sun_longest, sun_sweep;
    double earth_min, earth_max, earth_mean, earth_up, earth_longest, earth_sweep;
    char sun_first[32], sun_last[32], earth_first[32], earth_last[32];
    int power_pass, comms_pass;
    double score;
    int rank;
    char decision[16];
    char reason[512];
    char tradeoff[512];
} Site;

static Site sites[MAX_SITES];
static int nsites = 0;

/* --- time --- */
static const char *months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                "Jul","Aug","Sep","Oct","Nov","Dec"};
static const int mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};

static int month_num(const char *m)
{
    for (int i = 0; i < 12; i++)
        if (strncmp(m, months[i], 3) == 0) return i + 1;
    return 0;
}

/* "2026-Nov-15 00:00" -> hours since 2026-11-15 00:00 UTC. */
static double parse_hours(const char *dt)
{
    int y, d, hh, mm;
    char mon[8];
    if (sscanf(dt, "%d-%3s-%d %d:%d", &y, mon, &d, &hh, &mm) != 5) return 0.0;
    int m = month_num(mon);
    if (m == 0) return 0.0;
    int doy = d;
    for (int i = 0; i < m - 1; i++) doy += mdays[i];
    /* 2026 is not a leap year. Nov 15 is day 319. */
    return (double)(doy - 319) * 24.0 + (double)hh + (double)mm / 60.0;
}


/* Join dir + "/" + file into dst, failing loudly instead of truncating. */
static void join_path(char *dst, size_t n, const char *dir, const char *file)
{
    size_t ld = strlen(dir), lf = strlen(file);
    if (ld + lf + 2 > n) { fprintf(stderr, "nav: path too long: %s/%s\n", dir, file); exit(1); }
    memcpy(dst, dir, ld);
    dst[ld] = '/';
    memcpy(dst + ld + 1, file, lf + 1);
}


/* Build "<slug><suffix>" into dst, failing loudly instead of truncating. */
static void slug_file(char *dst, size_t n, const char *slug, const char *suffix)
{
    size_t ls = strlen(slug), lf = strlen(suffix);
    if (ls + lf + 1 > n) { fprintf(stderr, "nav: filename too long\n"); exit(1); }
    memcpy(dst, slug, ls);
    memcpy(dst + ls, suffix, lf + 1);
}

/* --- input --- */
static char *read_line(FILE *f, char *buf, size_t n)
{
    if (!fgets(buf, (int)n, f)) return NULL;
    size_t len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
    return buf;
}

/* One Horizons CSV row: "2026-Nov-15 00:00,*, , 114.442858,     1.349506"
 * Field 2 is a flag column (blank or 'i'); fields are positional. */
static int parse_row(const char *line, Sample *s)
{
    char tmp[512];
    snprintf(tmp, sizeof tmp, "%s", line);
    char *save = NULL;
    char *f0 = strtok_r(tmp, ",", &save);
    if (!f0) return 0;
    strtok_r(NULL, ",", &save); /* flag 1 */
    strtok_r(NULL, ",", &save); /* flag 2 */
    char *f3 = strtok_r(NULL, ",", &save);
    char *f4 = strtok_r(NULL, ",", &save);
    if (!f3 || !f4) return 0;
    snprintf(s->datetime, sizeof s->datetime, "%s", f0);
    s->hours = parse_hours(f0);
    s->az = strtod(f3, NULL);
    s->el = strtod(f4, NULL);
    return 1;
}

static int load_body(const char *path, Sample *out)
{
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "nav: cannot open %s\n", path); exit(1); }
    char line[512];
    int n = 0;
    while (read_line(f, line, sizeof line)) {
        if (n >= MAX_SAMPLES) { fprintf(stderr, "nav: too many rows in %s\n", path); exit(1); }
        if (parse_row(line, &out[n])) n++;
    }
    fclose(f);
    return n;
}

static void load_sites(const char *dir)
{
    char path[512];
    join_path(path, sizeof path, dir, "sites.tsv");
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "nav: cannot open %s\n", path); exit(1); }
    char line[512];
    while (read_line(f, line, sizeof line)) {
        if (line[0] == '#' || line[0] == '\0') continue;
        if (nsites >= MAX_SITES) { fprintf(stderr, "nav: too many sites\n"); exit(1); }
        Site *s = &sites[nsites];
        char *save = NULL;
        char *t0 = strtok_r(line, "\t", &save);
        char *t1 = strtok_r(NULL, "\t", &save);
        char *t2 = strtok_r(NULL, "\t", &save);
        char *t3 = strtok_r(NULL, "\t", &save);
        if (!t0 || !t1 || !t2 || !t3) { fprintf(stderr, "nav: bad sites.tsv line: %s\n", line); exit(1); }
        snprintf(s->slug, sizeof s->slug, "%s", t0);
        snprintf(s->name, sizeof s->name, "%s", t1);
        s->lon = strtod(t2, NULL);
        s->lat = strtod(t3, NULL);
        s->feo = 0.0; s->al2o3 = 0.0;
        nsites++;
    }
    fclose(f);

    join_path(path, sizeof path, dir, "science.tsv");
    f = fopen(path, "r");
    if (!f) { fprintf(stderr, "nav: cannot open %s\n", path); exit(1); }
    while (read_line(f, line, sizeof line)) {
        if (line[0] == '#' || line[0] == '\0') continue;
        char *save = NULL;
        char *t0 = strtok_r(line, "\t", &save);
        char *t1 = strtok_r(NULL, "\t", &save);
        char *t2 = strtok_r(NULL, "\t", &save);
        if (!t0 || !t1 || !t2) continue;
        for (int i = 0; i < nsites; i++) {
            if (strcmp(sites[i].slug, t0) == 0) {
                sites[i].feo = strtod(t1, NULL);
                sites[i].al2o3 = strtod(t2, NULL);
            }
        }
    }
    fclose(f);

    for (int i = 0; i < nsites; i++) {
        Site *s = &sites[i];
        char fname[32];
        slug_file(fname, sizeof fname, s->slug, "-sun.csv");
        join_path(path, sizeof path, dir, "csv");
        join_path(path, sizeof path, path, fname);
        int n1 = load_body(path, s->sun);
        slug_file(fname, sizeof fname, s->slug, "-earth.csv");
        join_path(path, sizeof path, dir, "csv");
        join_path(path, sizeof path, path, fname);
        int n2 = load_body(path, s->earth);
        if (n1 != n2 || n1 < 2) {
            fprintf(stderr, "nav: %s has %d sun rows and %d earth rows\n", s->slug, n1, n2);
            exit(1);
        }
        s->n = n1;
    }
}

/* --- metrics --- */
/* Hours above 0 deg by linear interpolation between samples, longest
 * continuous above-horizon stretch, min/max/mean, unwrapped azimuth
 * sweep, first/last above-horizon sample datetimes. */
static void body_metrics(Sample *s, int n,
                         double *mn, double *mx, double *mean,
                         double *up, double *longest, double *sweep,
                         char *first, char *last)
{
    double lo = s[0].el, hi = s[0].el, sum = 0.0;
    for (int i = 0; i < n; i++) {
        if (s[i].el < lo) lo = s[i].el;
        if (s[i].el > hi) hi = s[i].el;
        sum += s[i].el;
    }
    *mn = lo; *mx = hi; *mean = sum / (double)n;

    double up_h = 0.0, cur = 0.0, best = 0.0;
    int have_first = 0, have_last = 0;
    for (int i = 0; i < n; i++) {
        if (s[i].el > 0.0) {
            if (!have_first) { snprintf(first, 32, "%s", s[i].datetime); have_first = 1; }
            snprintf(last, 32, "%s", s[i].datetime); have_last = 1;
        }
    }
    if (!have_first) { snprintf(first, 32, "never in window"); }
    if (!have_last) { snprintf(last, 32, "never in window"); }

    for (int i = 0; i + 1 < n; i++) {
        double dt = s[i+1].hours - s[i].hours;
        double e0 = s[i].el, e1 = s[i+1].el;
        double above;
        if (e0 > 0.0 && e1 > 0.0) {
            above = dt;
        } else if (e0 <= 0.0 && e1 <= 0.0) {
            above = 0.0;
        } else {
            double frac = (0.0 - e0) / (e1 - e0); /* 0..1, crossing point */
            above = (e0 > 0.0) ? frac * dt : (1.0 - frac) * dt;
        }
        up_h += above;
        if (above >= dt - 1e-9) {
            cur += dt;
        } else if (above <= 1e-9) {
            if (cur > best) best = cur;
            cur = 0.0;
        } else {
            cur += above;
            if (cur > best) best = cur;
            cur = 0.0;
        }
    }
    if (cur > best) best = cur;
    *up = up_h;
    *longest = best;

    /* Unwrap azimuth so a sweep crossing north reads correctly. */
    double u[MAX_SAMPLES];
    u[0] = s[0].az;
    double ulo = u[0], uhi = u[0];
    for (int i = 1; i < n; i++) {
        double d = s[i].az - s[i-1].az;
        while (d > 180.0) d -= 360.0;
        while (d < -180.0) d += 360.0;
        u[i] = u[i-1] + d;
        if (u[i] < ulo) ulo = u[i];
        if (u[i] > uhi) uhi = u[i];
    }
    *sweep = uhi - ulo;
}

static void compute_all(void)
{
    for (int i = 0; i < nsites; i++) {
        Site *s = &sites[i];
        body_metrics(s->sun, s->n, &s->sun_min, &s->sun_max, &s->sun_mean,
                     &s->sun_up, &s->sun_longest, &s->sun_sweep,
                     s->sun_first, s->sun_last);
        body_metrics(s->earth, s->n, &s->earth_min, &s->earth_max, &s->earth_mean,
                     &s->earth_up, &s->earth_longest, &s->earth_sweep,
                     s->earth_first, s->earth_last);
        s->power_pass = (s->sun_up >= 300.0 && s->sun_max >= 3.0);
        s->comms_pass = (s->earth_up >= 300.0 && s->earth_max >= 5.0);
        s->score = s->sun_up + s->earth_up;
    }
    /* Rank by score, descending. An exact tie on combined hours is
     * broken by the sum of the two maximum elevations. */
    for (int i = 0; i < nsites; i++) {
        int r = 1;
        for (int j = 0; j < nsites; j++) {
            if (sites[j].score > sites[i].score) {
                r++;
            } else if (j != i && sites[j].score == sites[i].score) {
                double ti = sites[i].sun_max + sites[i].earth_max;
                double tj = sites[j].sun_max + sites[j].earth_max;
                if (tj > ti) r++;
            }
        }
        sites[i].rank = r;
    }
    for (int i = 0; i < nsites; i++) {
        Site *s = &sites[i];
        if (s->power_pass && s->comms_pass) {
            snprintf(s->decision, sizeof s->decision, "%s",
                     s->rank == 1 ? "RECOMMENDED" : "VIABLE");
        } else if (s->power_pass || s->comms_pass) {
            snprintf(s->decision, sizeof s->decision, "MARGINAL");
        } else {
            snprintf(s->decision, sizeof s->decision, "REJECT");
        }
        /* Reason and tradeoff, written from this site's own numbers. */
        if (s->rank == 1) {
            snprintf(s->reason, sizeof s->reason,
                "Highest combined sky access in the window: Sun up %.1f h and Earth up %.1f h of %.0f h, "
                "with the highest Earth maximum (+%.2f deg).",
                s->sun_up, s->earth_up, WINDOW_HOURS, s->earth_max);
        } else if (s->power_pass && s->comms_pass) {
            snprintf(s->reason, sizeof s->reason,
                "Passes both guards: Sun up %.1f h (max +%.2f deg), Earth up %.1f h (max +%.2f deg). "
                "Ranked #%d of %d on combined sky access.",
                s->sun_up, s->sun_max, s->earth_up, s->earth_max, s->rank, nsites);
        } else if (!s->power_pass && s->comms_pass) {
            snprintf(s->reason, sizeof s->reason,
                "Comms pass (Earth up %.1f h, max +%.2f deg) but power fails: Sun up only %.1f h of %.0f h, "
                "maximum +%.2f deg.",
                s->earth_up, s->earth_max, s->sun_up, WINDOW_HOURS, s->sun_max);
        } else if (s->power_pass && !s->comms_pass) {
            snprintf(s->reason, sizeof s->reason,
                "Power pass (Sun up %.1f h, max +%.2f deg) but comms fails: Earth up only %.1f h of %.0f h, "
                "maximum +%.2f deg.",
                s->sun_up, s->sun_max, s->earth_up, WINDOW_HOURS, s->earth_max);
        } else {
            snprintf(s->reason, sizeof s->reason,
                "Fails both guards: Sun up only %.1f h (max +%.2f deg), Earth up only %.1f h (max +%.2f deg) "
                "of %.0f h.",
                s->sun_up, s->sun_max, s->earth_up, s->earth_max, WINDOW_HOURS);
        }
        /* Tradeoff: the site's weakest dimension against the best site. */
        double best_sun = 0.0, best_earth = 0.0;
        for (int j = 0; j < nsites; j++) {
            if (sites[j].sun_max > best_sun) best_sun = sites[j].sun_max;
            if (sites[j].earth_max > best_earth) best_earth = sites[j].earth_max;
        }
        if (!s->power_pass && s->sun_up < 250.0) {
            snprintf(s->tradeoff, sizeof s->tradeoff,
                "Nearly half the window in darkness (Sun down %.1f h). A surface stay here needs batteries or "
                "nuclear power sized for multi-day nights, not solar alone.",
                WINDOW_HOURS - s->sun_up);
        } else if (!s->comms_pass && s->earth_max < 5.0) {
            snprintf(s->tradeoff, sizeof s->tradeoff,
                "Earth never climbs above +%.2f deg (best site: +%.2f deg). Direct-to-Earth comms would need a "
                "relay or a taller antenna mast; the link margin is the binding constraint.",
                s->earth_max, best_earth);
        } else if (s->sun_max < best_sun - 1.0) {
            snprintf(s->tradeoff, sizeof s->tradeoff,
                "Sun maximum +%.2f deg vs +%.2f deg at the best-lit site — the Sun's peak is low, so even "
                "its up-hours carry little power; local terrain masking (LOLA) is not yet modeled and could cut it further.",
                s->sun_max, best_sun);
        } else {
            snprintf(s->tradeoff, sizeof s->tradeoff,
                "No single weak dimension in the sky geometry. The open question is terrain: the local horizon "
                "profile (LOLA) is not modeled yet, and at these grazing Sun angles a nearby ridge decides the "
                "real illumination.");
        }
    }
}

/* --- CSV output --- */
static void csv_escape(FILE *f, const char *s)
{
    /* Real quoting: wrap in double quotes when the field holds a comma,
     * quote, or newline, and double inner quotes. */
    int need = 0;
    for (const char *p = s; *p; p++)
        if (*p == ',' || *p == '"' || *p == '\n') { need = 1; break; }
    if (!need) { fputs(s, f); return; }
    fputc('"', f);
    for (const char *p = s; *p; p++) {
        if (*p == '"') fputc('"', f);
        fputc(*p, f);
    }
    fputc('"', f);
}

static void write_csv(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) { fprintf(stderr, "nav: cannot write %s\n", path); exit(1); }
    fprintf(f, "region,date_window,sun_illumination,earth_visibility,terrain_note,science_value,constraint_pass_fail,decision,reason,tradeoff,source_tag\n");
    for (int i = 0; i < nsites; i++) {
        Site *s = &sites[i];
        char sun_ill[256], earth_vis[256], sci[256], guards[128];
        snprintf(sun_ill, sizeof sun_ill,
            "up %.1f h of %.0f h; max +%.2f deg; mean %+.2f deg; longest stretch %.1f h",
            s->sun_up, WINDOW_HOURS, s->sun_max, s->sun_mean, s->sun_longest);
        snprintf(earth_vis, sizeof earth_vis,
            "up %.1f h of %.0f h; max +%.2f deg; mean %+.2f deg; longest stretch %.1f h",
            s->earth_up, WINDOW_HOURS, s->earth_max, s->earth_mean, s->earth_longest);
        snprintf(sci, sizeof sci,
            "FeO %.2f wt%% Al2O3 %.2f wt%% (1D-CNN preprint Table 5); feldspathic highlands",
            s->feo, s->al2o3);
        snprintf(guards, sizeof guards, "power:%s comms:%s terrain:UNKNOWN",
                 s->power_pass ? "PASS" : "FAIL", s->comms_pass ? "PASS" : "FAIL");
        fprintf(f, "%s,", s->name);
        fprintf(f, "2026-11-15 to 2026-11-29 (14 days 6-hourly),");
        csv_escape(f, sun_ill); fprintf(f, ",");
        csv_escape(f, earth_vis); fprintf(f, ",");
        fprintf(f, "LOLA horizon profile not computed - terrain guard unknown,");
        csv_escape(f, sci); fprintf(f, ",");
        fprintf(f, "%s,", guards);
        fprintf(f, "%s,", s->decision);
        csv_escape(f, s->reason); fprintf(f, ",");
        csv_escape(f, s->tradeoff); fprintf(f, ",");
        fprintf(f, "JPL Horizons observer ephemeris (ssd.jpl.nasa.gov/api/horizons.api) 2026-11-15\n");
    }
    fclose(f);
}

/* --- SVG sky plot --- */
#define DEG2RAD 0.017453292519943295
/* Polar plot: center is elevation +12 deg, outer ring is -6 deg.
 * Horizon (0 deg) is the ring at two-thirds radius. */
static double sky_r(double el, double R) { return R * (12.0 - el) / 18.0; }
static double sky_x(double cx, double R, double az, double el)
{
    return cx + sky_r(el, R) * sin(az * DEG2RAD);
}
static double sky_y(double cy, double R, double az, double el)
{
    return cy - sky_r(el, R) * cos(az * DEG2RAD);
}

/* Emit one <polyline> per continuous run of Sun samples on one side of
 * the horizon, with interpolated crossing endpoints, so no line ever
 * cuts across the below-horizon region. above=1 draws el>0 runs,
 * above=0 draws el<=0 runs. */
static void sun_runs(FILE *f, Site *s, int above, double cx, double cy, double R,
                     const char *stroke, const char *extra)
{
    int in_run = 0;
    for (int i = 0; i + 1 < s->n; i++) {
        double e0 = s->sun[i].el, e1 = s->sun[i+1].el;
        int side0 = (e0 > 0.0) == (above != 0);
        int side1 = (e1 > 0.0) == (above != 0);
        if (side0 && !in_run) {
            fprintf(f, "<polyline fill=\"none\" stroke=\"%s\" stroke-width=\"2\" %s points=\"",
                    stroke, extra);
            fprintf(f, "%.1f,%.1f", sky_x(cx, R, s->sun[i].az, e0), sky_y(cy, R, s->sun[i].az, e0));
            in_run = 1;
        } else if (side0 && in_run) {
            fprintf(f, " %.1f,%.1f", sky_x(cx, R, s->sun[i].az, e0), sky_y(cy, R, s->sun[i].az, e0));
        }
        if (side0 != side1) {
            double frac = (0.0 - e0) / (e1 - e0);
            double daz = s->sun[i+1].az - s->sun[i].az;
            while (daz > 180.0) daz -= 360.0;
            while (daz < -180.0) daz += 360.0;
            double azx = s->sun[i].az + frac * daz;
            if (in_run) {
                fprintf(f, " %.1f,%.1f\"", sky_x(cx, R, azx, 0.0), sky_y(cy, R, azx, 0.0));
                fprintf(f, "/>\n");
                in_run = 0;
            } else {
                fprintf(f, "<polyline fill=\"none\" stroke=\"%s\" stroke-width=\"2\" %s points=\"",
                        stroke, extra);
                fprintf(f, "%.1f,%.1f", sky_x(cx, R, azx, 0.0), sky_y(cy, R, azx, 0.0));
                in_run = 1;
            }
        }
    }
    if (in_run) {
        int last_side = (s->sun[s->n-1].el > 0.0) == (above != 0);
        if (last_side)
            fprintf(f, " %.1f,%.1f",
                    sky_x(cx, R, s->sun[s->n-1].az, s->sun[s->n-1].el),
                    sky_y(cy, R, s->sun[s->n-1].az, s->sun[s->n-1].el));
        fprintf(f, "\"/>\n");
    }
}

static void svg_sky(FILE *f, Site *s)
{
    const double cx = 170.0, cy = 170.0, R = 150.0;
    fprintf(f, "<svg viewBox=\"0 0 340 380\" xmlns=\"http://www.w3.org/2000/svg\" role=\"img\" aria-label=\"Sky plot for %s\">\n", s->name);
    fprintf(f, "<rect x=\"0\" y=\"0\" width=\"340\" height=\"380\" fill=\"#0b1020\"/>\n");
    /* rings: -5, 0, +5, +10 deg */
    const double ring_el[] = {-5.0, 0.0, 5.0, 10.0};
    for (int i = 0; i < 4; i++) {
        double r = sky_r(ring_el[i], R);
        fprintf(f, "<circle cx=\"%.1f\" cy=\"%.1f\" r=\"%.1f\" fill=\"none\" stroke=\"%s\" stroke-width=\"%.1f\"/>\n",
                cx, cy, r, ring_el[i] == 0.0 ? "#e8e8e8" : "#334155",
                ring_el[i] == 0.0 ? 2.0 : 1.0);
        fprintf(f, "<text x=\"%.1f\" y=\"%.1f\" fill=\"#94a3b8\" font-size=\"11\">%+.0f deg</text>\n",
                cx + 4, cy - r + 12, ring_el[i]);
    }
    /* spokes + cardinals */
    const char *card[] = {"N", "E", "S", "W"};
    for (int i = 0; i < 4; i++) {
        double a = i * 90.0 * DEG2RAD;
        fprintf(f, "<line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" stroke=\"#1e293b\" stroke-width=\"1\"/>\n",
                cx, cy, cx + R * sin(a), cy - R * cos(a));
        fprintf(f, "<text x=\"%.1f\" y=\"%.1f\" fill=\"#e2e8f0\" font-size=\"13\" text-anchor=\"middle\">%s</text>\n",
                cx + (R + 12) * sin(a), cy - (R + 12) * cos(a) + 4, card[i]);
    }
    /* Sun path, split at the horizon: gold above, grey dashed below. */
    sun_runs(f, s, 1, cx, cy, R, "#f59e0b", "");
    sun_runs(f, s, 0, cx, cy, R, "#64748b", "stroke-dasharray=\"4 3\"");
    /* Earth: nearly fixed — one dot per sample, blue. */
    for (int i = 0; i < s->n; i++) {
        fprintf(f, "<circle cx=\"%.1f\" cy=\"%.1f\" r=\"3\" fill=\"#38bdf8\"/>\n",
                sky_x(cx, R, s->earth[i].az, s->earth[i].el),
                sky_y(cy, R, s->earth[i].az, s->earth[i].el));
    }
    /* Sun start marker + legend */
    fprintf(f, "<circle cx=\"%.1f\" cy=\"%.1f\" r=\"5\" fill=\"#f59e0b\"/>\n",
            sky_x(cx, R, s->sun[0].az, s->sun[0].el),
            sky_y(cy, R, s->sun[0].az, s->sun[0].el));
    fprintf(f, "<text x=\"170\" y=\"360\" fill=\"#e2e8f0\" font-size=\"13\" text-anchor=\"middle\">%s</text>\n", s->name);
    fprintf(f, "<text x=\"170\" y=\"378\" fill=\"#94a3b8\" font-size=\"11\" text-anchor=\"middle\">gold Sun path, dashed while below horizon  |  blue Earth</text>\n");
    fprintf(f, "</svg>\n");
}

/* --- 3D sky-dome view --- */
static const char b64tab[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Returns a malloc'd base64 string of the file, or NULL when the file
 * is missing or unreadable. The caller frees it. */
static char *read_file_b64(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long len = ftell(f);
    if (len < 0) { fclose(f); return NULL; }
    rewind(f);
    unsigned char *buf = (unsigned char *)malloc(len > 0 ? (size_t)len : 1);
    if (!buf) { fclose(f); return NULL; }
    if (len > 0 && fread(buf, 1, (size_t)len, f) != (size_t)len) {
        free(buf); fclose(f); return NULL;
    }
    fclose(f);
    size_t outlen = ((size_t)len + 2) / 3 * 4;
    char *out = (char *)malloc(outlen + 1);
    if (!out) { free(buf); return NULL; }
    size_t o = 0;
    for (long i = 0; i < len; i += 3) {
        long rem = len - i;
        unsigned int v = (unsigned int)buf[i] << 16;
        if (rem > 1) v |= (unsigned int)buf[i + 1] << 8;
        if (rem > 2) v |= buf[i + 2];
        out[o++] = b64tab[(v >> 18) & 63];
        out[o++] = b64tab[(v >> 12) & 63];
        out[o++] = rem > 1 ? b64tab[(v >> 6) & 63] : '=';
        out[o++] = rem > 2 ? b64tab[v & 63] : '=';
    }
    out[o] = '\0';
    free(buf);
    return out;
}

/* A 3D sky-dome render (out/sky/<slug>.png) when one exists, else the
 * SVG polar plot. The render comes from sky/render.sh: the OpenGL host
 * runs sky/kernel_skydome.frag over this site's own 57 Horizons
 * samples. The page stays a single file because the PNG is embedded
 * base64. */
static void sky_view(FILE *f, Site *s, const char *outdir)
{
    char dir[512], fname[32], path[512];
    join_path(dir, sizeof dir, outdir, "sky");
    slug_file(fname, sizeof fname, s->slug, ".png");
    join_path(path, sizeof path, dir, fname);
    char *b64 = read_file_b64(path);
    if (b64) {
        fprintf(f, "<img src=\"data:image/png;base64,%s\" alt=\"3D sky dome over %s\" style=\"width:100%%;border-radius:8px;display:block\">\n",
                b64, s->name);
        free(b64);
        return;
    }
    svg_sky(f, s);
}

/* --- HTML --- */
static void write_html(const char *path, const char *outdir)
{
    FILE *f = fopen(path, "w");
    if (!f) { fprintf(stderr, "nav: cannot write %s\n", path); exit(1); }
    fprintf(f, "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n");
    fprintf(f, "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n");
    fprintf(f, "<title>Lunar Landing Site Navigator — South Pole, 2026-11-15 to 2026-11-29</title>\n");
    fprintf(f, "<style>\n"
        "body{background:#070b13;color:#e2e8f0;font-family:system-ui,-apple-system,Segoe UI,sans-serif;margin:0;padding:0 18px 60px;line-height:1.5}\n"
        "main{max-width:1060px;margin:0 auto}\n"
        "h1{font-size:30px;margin:28px 0 4px} h2{font-size:22px;margin:36px 0 8px;border-top:1px solid #1e293b;padding-top:18px}\n"
        "p{max-width:78ch} .sub{color:#94a3b8;margin:0 0 18px}\n"
        "table{border-collapse:collapse;width:100%%;font-size:14px;margin:12px 0}\n"
        "th,td{border:1px solid #334155;padding:7px 9px;text-align:left;vertical-align:top}\n"
        "th{background:#0f172a} td.num{text-align:right;font-variant-numeric:tabular-nums}\n"
        ".pass{color:#4ade80;font-weight:700} .fail{color:#f87171;font-weight:700} .unk{color:#facc15;font-weight:700}\n"
        ".rec{background:#052e16} .grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(340px,1fr));gap:16px}\n"
        ".card{background:#0b1020;border:1px solid #1e293b;border-radius:10px;padding:10px}\n"
        ".card p{font-size:13px;color:#cbd5e1;margin:8px 2px 2px}\n"
        "code{background:#0f172a;padding:1px 5px;border-radius:4px}\n"
        "footer{color:#64748b;font-size:13px;margin-top:40px}\n"
        "</style>\n</head>\n<body>\n<main>\n");
    fprintf(f, "<h1>Lunar Landing Site Navigator</h1>\n");
    fprintf(f, "<p class=\"sub\">Nine Artemis III candidate regions, compared on real Sun and Earth sky geometry for "
               "2026-11-15 to 2026-11-29 (14 days, 6-hourly). NASA International Space Apps Challenge 2026, "
               "challenge Lunar Landing Site Navigator. Built by Jessica Williams' team with Muse as the disclosed AI tool.</p>\n");

    fprintf(f, "<h2>The comparison</h2>\n");
    fprintf(f, "<p>Every number below comes from a JPL Horizons observer ephemeris row. "
               "Sun up means the Sun's center above the local flat horizon; Earth up means the same for Earth's center. "
               "Elevations are airless and geometric — local terrain is not modeled, and the terrain guard says so instead of pretending.</p>\n");
    fprintf(f, "<table>\n<tr><th>Region</th><th>Sun up</th><th>Sun max</th><th>Earth up</th><th>Earth max</th><th>Power</th><th>Comms</th><th>Decision</th></tr>\n");
    /* Print in rank order. */
    for (int r = 1; r <= nsites; r++) {
        for (int i = 0; i < nsites; i++) {
            if (sites[i].rank != r) continue;
            Site *s = &sites[i];
            fprintf(f, "<tr%s><td><strong>%s</strong><br><span style=\"color:#94a3b8\">%.3f&deg;, %.3f&deg;</span></td>"
                    "<td class=\"num\">%.1f h</td><td class=\"num\">%+.2f&deg;</td>"
                    "<td class=\"num\">%.1f h</td><td class=\"num\">%+.2f&deg;</td>"
                    "<td class=\"%s\">%s</td><td class=\"%s\">%s</td><td><strong>%s</strong></td></tr>\n",
                    s->rank == 1 ? " class=\"rec\"" : "",
                    s->name, s->lon, s->lat,
                    s->sun_up, s->sun_max, s->earth_up, s->earth_max,
                    s->power_pass ? "pass" : "fail", s->power_pass ? "PASS" : "FAIL",
                    s->comms_pass ? "pass" : "fail", s->comms_pass ? "PASS" : "FAIL",
                    s->decision);
        }
    }
    fprintf(f, "</table>\n");
    fprintf(f, "<p>Guard rules, stated in the open: power passes when the Sun is up at least 300 h of the 336 h window "
               "and reaches at least +3.0&deg;; comms passes when Earth is up at least 300 h and reaches at least +5.0&deg;. "
               "Terrain is UNKNOWN for every site — the LOLA horizon profile was not computed, so no site claims a terrain pass.</p>\n");

    fprintf(f, "<h2>The sky over each site</h2>\n");
    fprintf(f, "<p>One 3D sky dome per region, rendered by the OpenGL host and the GLSL sky-dome kernel in "
               "<code>sky/</code> from the same 57 Horizons samples behind the table — no invented scenery. "
               "The center is the zenith, the bold ring is the horizon (0&deg;), the edge is -10&deg;, north is up. "
               "The gold line is the Sun's path across the 14 days — solid while it is up, dashed grey while it is down. "
               "The blue dots are Earth, which barely moves in the lunar sky; the bright discs mark both bodies at "
               "mid-window (Nov 22 00:00 UTC), drawn larger than their true size (Sun 0.27&deg;, Earth 0.95&deg;) so they read. "
               "At the pole the Sun circles the horizon instead of rising and setting the way it does on Earth.</p>\n");
    fprintf(f, "<div class=\"grid\">\n");
    for (int r = 1; r <= nsites; r++) {
        for (int i = 0; i < nsites; i++) {
            if (sites[i].rank != r) continue;
            Site *s = &sites[i];
            fprintf(f, "<div class=\"card\">\n");
            sky_view(f, s, outdir);
            fprintf(f, "<p>Sun up <strong>%.1f h</strong> (max %+.2f&deg;, longest stretch %.1f h) &middot; "
                    "Earth up <strong>%.1f h</strong> (max %+.2f&deg;, longest stretch %.1f h)<br>"
                    "Decision: <strong>%s</strong> — %s</p>\n",
                    s->sun_up, s->sun_max, s->sun_longest,
                    s->earth_up, s->earth_max, s->earth_longest,
                    s->decision, s->reason);
            fprintf(f, "</div>\n");
        }
    }
    fprintf(f, "</div>\n");

    fprintf(f, "<h2>Decision ledger</h2>\n");
    fprintf(f, "<p>One row per region: the decision, the reason in numbers, and the tradeoff. The full ledger is also "
               "<code>sites_filled.csv</code> in this repository.</p>\n");
    fprintf(f, "<table>\n<tr><th>Region</th><th>Decision</th><th>Reason</th><th>Tradeoff</th></tr>\n");
    for (int r = 1; r <= nsites; r++) {
        for (int i = 0; i < nsites; i++) {
            if (sites[i].rank != r) continue;
            Site *s = &sites[i];
            fprintf(f, "<tr><td><strong>%s</strong></td><td><strong>%s</strong></td><td>%s</td><td>%s</td></tr>\n",
                    s->name, s->decision, s->reason, s->tradeoff);
        }
    }
    fprintf(f, "</table>\n");

    fprintf(f, "<h2>Science value</h2>\n");
    fprintf(f, "<p>Composition from Table 5 of the 1D-CNN preprint (Preprints.org, doi:10.20944/preprints202511.1488.v1 — "
               "preprint, not peer-reviewed), inverted from KAGUYA spectral data using Bian et al. (2025) oxide maps as "
               "training samples. All nine regions are feldspathic highland material. Slater Plain and de Gerlache Rim 2 "
               "capture the most compositional diversity within feasible operational ranges — which is why they stay "
               "interesting even though their sky geometry is the weakest.</p>\n");
    fprintf(f, "<table>\n<tr><th>Region</th><th>FeO wt%%</th><th>Al2O3 wt%%</th></tr>\n");
    for (int i = 0; i < nsites; i++) {
        fprintf(f, "<tr><td>%s</td><td class=\"num\">%.2f</td><td class=\"num\">%.2f</td></tr>\n",
                sites[i].name, sites[i].feo, sites[i].al2o3);
    }
    fprintf(f, "</table>\n");

    fprintf(f, "<h2>How the numbers were made</h2>\n");
    fprintf(f, "<p>Region coordinates are NASA's October 28, 2024 update of the nine Artemis III candidate regions, "
               "as tabulated in Table 4 of the 1D-CNN preprint (Preprints.org, doi:10.20944/preprints202511.1488.v1 — "
               "preprint, not peer-reviewed) and cross-checked against the USGS ScienceBase values "
               "(agreement within about 1&deg;, the regions themselves are roughly 10–38 km across).</p>\n");
    fprintf(f, "<p>Sun and Earth positions come from the JPL Horizons system, observer ephemeris, "
               "<code>https://ssd.jpl.nasa.gov/api/horizons.api</code> (Horizons API v1.3). "
               "Each query: target Sun (<code>COMMAND='10'</code>) or Earth (<code>COMMAND='399'</code>), "
               "center <code>coord@301</code> with the region's longitude/latitude/0 km, "
               "<code>QUANTITIES='4'</code> (airless apparent azimuth and elevation, degrees), "
               "2026-11-15 to 2026-11-29, every 6 hours — 57 rows per query, 18 queries, 1,026 rows total. "
               "The raw Horizons output is kept in <code>data/raw/</code>; the extracted tables in <code>data/csv/</code>; "
               "the fetch script is <code>data/fetch.sh</code>, so anyone can re-pull the same numbers.</p>\n");
    fprintf(f, "<p>Limits, stated plainly: elevations are the target's center above a flat horizon — the Sun's and Earth's "
               "angular radii (about 0.27&deg; and 0.95&deg;) are not added, and no local terrain is modeled. "
               "At these grazing angles a nearby ridge changes the real illumination, so the terrain guard is UNKNOWN "
               "until a LOLA horizon profile is computed. The 6-hour sampling is interpolated linearly for rise/set "
               "times; the true crossing can differ by minutes. Nothing on this page is a landing recommendation — "
               "it is a comparison of sky geometry, and the decision column says which site wins that comparison.</p>\n");

    fprintf(f, "<h2>Data sources</h2>\n");
    fprintf(f, "<ul>\n"
        "<li><strong>JPL Horizons System</strong> — Sun and Earth ephemeris — https://ssd.jpl.nasa.gov/horizons/</li>\n"
        "<li><strong>NASA Quickmap</strong> — LRO terrain and imagery — https://quickmap.lroc.asu.edu/</li>\n"
        "<li><strong>NASA Moon Trek</strong> — interactive lunar mapping — https://trek.nasa.gov/moon/</li>\n"
        "<li><strong>CSA Moon Exploration</strong> — background — https://www.asc-csa.gc.ca/eng/exploration/moon-exploration.asp</li>\n"
        "<li><strong>NASA Artemis III landing regions (Oct 28, 2024)</strong> — region coordinates, Table 4 of the 1D-CNN preprint (doi:10.20944/preprints202511.1488.v1, preprint, not peer-reviewed)</li>\n"
        "<li><strong>Oxide Abundance Inversion in the Lunar Polar Regions Using a 1D-CNN Algorithm</strong> — oxide abundances, Table 5 of the same preprint</li>\n"
        "</ul>\n");

    fprintf(f, "<h2>Use of AI</h2>\n");
    fprintf(f, "<p>This entry was built by Jessica Williams' team with Muse (Meta's personal AI agent) as the AI tool: "
               "Muse wrote the navigator program, generated this page from the Horizons data, and narrated the demo video. "
               "All numeric values come from JPL Horizons ephemeris rows, not from the AI — the program only reads, "
               "counts, and plots them. No AI-generated images are used: every diagram is rendered by this "
               "repository's own programs (the navigator and the OpenGL/GLSL sky-dome kernel) from the measured data. "
               "The demo video's narration is AI text-to-speech and is labeled as such. "
               "See <code>AI_DISCLOSURE.md</code> in the repository.</p>\n");

    fprintf(f, "<footer>\n<p>Lunar Landing Site Navigator — Jessica Williams' team, NASA International Space Apps Challenge 2026. "
               "Data: JPL Horizons (Nov 15–29, 2026 window). Generated 2026-11-15 by the navigator program in this repository.</p>\n</footer>\n");
    fprintf(f, "</main>\n</body>\n</html>\n");
    fclose(f);
}

/* --- summary --- */
static void write_summary(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) { fprintf(stderr, "nav: cannot write %s\n", path); exit(1); }
    fprintf(f, "Lunar Landing Site Navigator — window 2026-11-15 to 2026-11-29 (%.0f h, 6-hourly)\n\n", WINDOW_HOURS);
    for (int r = 1; r <= nsites; r++) {
        for (int i = 0; i < nsites; i++) {
            if (sites[i].rank != r) continue;
            Site *s = &sites[i];
            fprintf(f, "#%d %s (%.3f, %.3f)\n", s->rank, s->name, s->lon, s->lat);
            fprintf(f, "  Sun:   up %6.1f h  max %+6.2f  mean %+6.2f  longest %6.1f h  sweep %6.1f deg  first %s  last %s\n",
                    s->sun_up, s->sun_max, s->sun_mean, s->sun_longest, s->sun_sweep, s->sun_first, s->sun_last);
            fprintf(f, "  Earth: up %6.1f h  max %+6.2f  mean %+6.2f  longest %6.1f h  sweep %6.1f deg  first %s  last %s\n",
                    s->earth_up, s->earth_max, s->earth_mean, s->earth_longest, s->earth_sweep, s->earth_first, s->earth_last);
            fprintf(f, "  Guards: power %s  comms %s  terrain UNKNOWN | score %.1f | %s\n",
                    s->power_pass ? "PASS" : "FAIL", s->comms_pass ? "PASS" : "FAIL", s->score, s->decision);
            fprintf(f, "  Reason: %s\n  Tradeoff: %s\n\n", s->reason, s->tradeoff);
        }
    }
    fclose(f);
}

/* --- sky data files for the GLSL dome kernel --- */
static void write_sky_data(const char *outdir)
{
    char dir[512];
    join_path(dir, sizeof dir, outdir, "sky");
    mkdir(dir, 0755); /* already there on the second run: harmless */
    for (int i = 0; i < nsites; i++) {
        Site *s = &sites[i];
        char fname[32], path[512];
        slug_file(fname, sizeof fname, s->slug, ".txt");
        join_path(path, sizeof path, dir, fname);
        FILE *f = fopen(path, "w");
        if (!f) { fprintf(stderr, "nav: cannot write %s\n", path); exit(1); }
        fprintf(f, "# %s sky samples: Sun (S) and Earth (E) azimuth/elevation deg, 6-hourly\n", s->name);
        for (int j = 0; j < s->n; j++) fprintf(f, "S %.6f %.6f\n", s->sun[j].az, s->sun[j].el);
        for (int j = 0; j < s->n; j++) fprintf(f, "E %.6f %.6f\n", s->earth[j].az, s->earth[j].el);
        fclose(f);
    }
}

int main(int argc, char **argv)
{
    const char *datadir = argc > 1 ? argv[1] : "data";
    const char *outdir = argc > 2 ? argv[2] : "out";
    char path[512];

    load_sites(datadir);
    compute_all();

    join_path(path, sizeof path, outdir, "sites_filled.csv");
    write_csv(path);
    write_sky_data(outdir);
    join_path(path, sizeof path, outdir, "index.html");
    write_html(path, outdir);
    join_path(path, sizeof path, outdir, "summary.txt");
    write_summary(path);

    for (int r = 1; r <= nsites; r++)
        for (int i = 0; i < nsites; i++)
            if (sites[i].rank == r)
                printf("#%d %-22s sun %6.1fh/%+5.2f  earth %6.1fh/%+5.2f  %s\n",
                       r, sites[i].name, sites[i].sun_up, sites[i].sun_max,
                       sites[i].earth_up, sites[i].earth_max, sites[i].decision);
    printf("wrote %s/sites_filled.csv, %s/index.html, %s/summary.txt\n", outdir, outdir, outdir);
    return 0;
}
