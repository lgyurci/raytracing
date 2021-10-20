//Lázár György, Neptun: LIJIA8
#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <sys/time.h>
#include <pthread.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif
struct point { //Egy pont a térben
	double x,y,z;
};
struct vector { //egy vektor a térben
    double A,B,C;
};
struct function {
		char *expression;
};

struct function3d {
		struct function x;
		struct function y;
		struct function z;
};
struct ray { //egy sugár: egyenesként van definiálva, de gyakran félegyenesként van kezelve
	struct point p;
    struct vector v;
    bool isLight; //Fénysugár-e
    int reflected; //tükröződések száma
};
struct sphere { //egy gömb a térben
	double r; //sugár
    struct point p; //középpont
    struct function3d fv; //középpont függvény
    bool isMirror; //tükör-e a gömb
};
struct sphereColl {  //gömb-egyenes metszéspontok
    struct point p1; //az egyenes definiált pontjához (a félegyenes kiindulópontjához) mindig p1 van közelebb
    struct point p2;
    bool h; //egyáltalán van-e metszéspont
    double angle; //az egyenes és a gömb p1 pontbeli érintője által bezárt szög
};
struct persp { //perspektíva: ez az a sugárhalmaz, amellyel az objektumok ütköznek, kialakítva ezzel a képet
    struct ray **r;
};
struct rayColl { //egyenes ütközése bármivel
    struct point p;
    double angle;
    bool h;
    bool isMirrored; //Visszatükrözi-e az adott ütközés a sugarat
    int type; //ütözés típusa: 1-gömb 2-sík 3-véges sík
    void *hitObj; //az egyenes által eltalált objektum
};
struct plane { //sík
    struct point p;
    struct vector v;
    bool isMirror;
};
struct finitePlane { //"véges" sík
    struct plane pl; //egy sík, amin fekszik a véges sík
    struct vector v1; //első kiterjedés vektora
    struct vector v2; //második kiterjedés vektora
    struct point p; //kiindulópont (v1 és v2 "innen indul")
};
struct objects { //az összes pályán szereplő objektum
    struct sphere *s; //gömb
    int scount; //gömbök száma
    struct plane *pl;
    int plcount;
    struct finitePlane *fpl;
    int fplcount;
};
struct rtArgs { //ray tracing szál argumentumai
    int pos; //hanyadik pixeltől induljon
    int jump; //az indulástól számítva hány pixelt ugorjon át
};
struct pixel { //egy pixel színe
    int r,g,b,a;
};

//függvények
double dotProduct(struct vector v1, struct vector v2); //skaláris szorzat
bool isNullV(struct vector v); //null vektor-e
double pDist(struct point p1, struct point p2); //két pont távolsága
double vAbs(struct vector v); //vektor abszolút értéke
struct vector crossProduct(struct vector v1, struct vector v2); //vektoriális szorzat
double vAngle(struct vector v1, struct vector v2); //két vektor által bezárt szög
struct vector vAdd(struct vector v1, struct vector v2); //két vektor összege
struct vector reduceTo(struct vector v, double targetLength); //vektor hosszát egy adott értékre állítja, az írányát nem változtatja
struct point vToP(struct vector v); //visszaadja a vektor végpontját
struct vector vMult(struct vector v1, double lambda); //vektor szorzása skalárral
struct sphereColl raySphereCollision(struct ray ray, struct sphere sphere); //egyenes-gömb ütközésének számolása
struct rayColl plColl(struct ray r, struct plane pl); //sík-egyenes ütközésének számolása
void calcPersp(struct ray r); //perspektíva kiszámolása
struct point transloc(struct point p, struct vector v); //pont eltolása egy adott vektorral
struct vector pToV(struct point p1,struct point p2); ///két pontból csinál vektort
double calcLSAngle(struct point p); //a fényforrás és a megadott pont által bezárt szöget számolja ki. Ha a fény nem jut el az adott helyre, 0-át ad vissza
struct rayColl engage (struct ray r); //egy sugár ütköztetése az összes objektummal
void printPoint(struct point p); //egy pont kiírása a standard outra (csak debug funkció)
void printVector(struct vector v); //egy vektor kiírása a standard outra (csak debug funkció)
double v2DAngle(double v1A, double v1B, double v2A, double v2B); //két 2D-s vektor által bezárt szöget adja vissza
bool isSuspiciousV(struct vector v); //"gyanús-e" egy vektor (inf érték vagy ilyesmi)
double strDou(char in[]); //szöveget konvertál double-be
struct point strCoor(char in[]); //(x,y,z) alakú szöveget konvertál ponttá
double recursiveParentheses(char af[],double t, int* index); //az eval functhoz kell
double evalFunctAt(struct function f,double t); //kiszámol egy függvényt t időben
int isNum(char a); //szám-e a karakter
struct function3d strFunc3d(char in[]); //stringből 3d függvény
struct point evalFunction3d(struct function3d f, double t);

char *mapFile; //map file

//globális változók
int height = 500; //ablak magassága
int width = 750; //ablak szélessége
double distance = 10; //A vezérsugár (perspektíva középső sugara) és a vezérpont (perspektíva egyeneseinek a metszéspontja) távolsága
double perspPixelSize = 0.02; //két sugár közötti távolság (egy pixel ekkora távolságnak felel meg a vezérsíkon
static struct persp pe; //perspektíva
double ambientLight = 0.3; //környezeti fényerősség (max 1, de nincs értelme 0.5 fölé menni)
struct point lightSource = {200,-200,200}; //fényforrás koordinátái
double lsStrength = 1; //fényforrás erőssége (max 1, nincs sok értelme kevesebbet megadni)
static struct objects o; //objektumok összessége
static struct pixel **pixels; //renderelt pixelek buffere
struct pixel background = {200,200,200,255}; //háttér színe
int maxRayReflect = 10; //maximális visszaverődés
bool sigterm = false; //program leállítása
struct vector currentSpeed = {0,0,0}; //jelenlegi sebesség
double maxSpeed = 25; //maximális sebesség
double rotSpeed = 0.6; //maximális forgási sebesség
struct vector currRotSpeed = {0,0,0}; //jelenlegi forgási sebesség
bool showFPS = false; //képkockasebesség kijelzése
bool reset = false; //perspektíva alaphelyzetbe állítása
int tcount = 11; //ray tracing szálak száma
struct ray startingMasterRay; //vezérsugár
bool showPos = false; //pozíció jelzése
double pollingRate = 10; //input mintavételezési sebessége/sec
long gt_zero = 0;
long gt = 0; //global time


void configure(char line[]){ //az adott szöveget próbálja meg konfigurációs beállításként értelmezni, és átállítani az annak megfelelő változót
    if (line[0] != '#') {
                char option[50];
                int ol = 0; //hol vagyunk most
                for (; line[ol] != ':' && line[ol] != '\0';ol++){
                    option[ol] = line[ol];
                }
                option[ol] = '\0'; //megkerestük a beállítás nevét
                if (strcmp(option,"Resolution") == 0) {
                    ol++;
                    char w[6];
                    char h[6];
                    for (int i = 0; i < 6; i++){
                        w[i] = 0;
                        h[i] = 0;
                    }
                    for(int i = 0; line[ol] != 'x'; ol++){
                        w[i++] = line[ol];
                    }
                    ol++;
                    for(int i = 0; line[ol] != ';'; ol++){
                        h[i++] = line[ol];
                    }
                    width = atoi(w);
                    height = atoi(h);
                    if (width == 0 || height == 0) {
                        printf("invalid resolution\n");
                        exit(0);
                    }
                    struct ray **tmp = malloc(sizeof(struct ray)*height);
                    for (int i = 0; i < height; i++){
                        tmp[i] = (struct ray *)malloc(sizeof(struct ray)*width);
                    }
                    pe.r = tmp;
                    struct pixel **tmp2 = malloc(sizeof(struct pixel)*height);
                    for (int i = 0; i < height; i++){
                        tmp2[i] = (struct pixel *)malloc(sizeof(struct pixel)*width);
                    }
                    pixels = tmp2;
                    for (int i = 0; i < height; i++){
                        for (int j = 0; j < width; j++){
                            pixels[i][j].a = 0;
                        }
                    }
                }
                if (strcmp(option,"RTThreads") == 0) {
                    char tc [6];
                    for (int i = 0; i < 6; i++){
                        tc[i] = 0;
                    }
                    ol++;
                    int i;
                    for(i = 0; line[ol] != ';' && line[ol] != '\0'; ol++){
                        tc[i++] = line[ol];
                    }
                    tc[i] = '\0';
                    tcount = atoi(tc);
                    if (tcount == 0) {
                        exit(0);
                    }
                }
                if (strcmp(option,"PerspectiveLength") == 0) {
                    char tc [6];
                    for (int i = 0; i < 6; i++){
                        tc[i] = 0;
                    }
                    ol++;
                    int i;
                    for(i = 0;line[ol] != ';' && line[ol] != '\0'; ol++){
                        tc[i++] = line[ol];
                    }
                    tc[i] = '\0';
                    distance = strDou(tc);
                }
                if (strcmp(option,"PixelSize") == 0) {
                    char tc [6];
                    for (int i = 0; i < 6; i++){
                        tc[i] = 0;
                    }
                    ol++;
                    int i;
                    for(i = 0;line[ol] != ';' && line[ol] != '\0'; ol++){
                        tc[i++] = line[ol];
                    }
                    tc[i] = '\0';
                    perspPixelSize = strDou(tc);
                }
                if (strcmp(option,"AmbientLightStrength") == 0) {
                    char tc [6];
                    for (int i = 0; i < 6; i++){
                        tc[i] = 0;
                    }
                    ol++;
                    int i;
                    for(i = 0;line[ol] != ';' && line[ol] != '\0'; ol++){
                        tc[i++] = line[ol];
                    }
                    tc[i] = '\0';
                    ambientLight = strDou(tc);
                }
                if (strcmp(option,"LightSourceStartingPos") == 0) {
                    char tc [60];
                    for (int i = 0; i < 60; i++){
                        tc[i] = 0;
                    }
                    ol++;
                    int i;
                    for(i = 0;line[ol] != ';' && line[ol] != '\0'; ol++){
                        tc[i++] = line[ol];
                    }
                    tc[i] = '\0';
                    lightSource = strCoor(tc);
                }
                if (strcmp(option,"LightSourceStrength") == 0) {
                    char tc [6];
                    for (int i = 0; i < 6; i++){
                        tc[i] = 0;
                    }
                    ol++;
                    int i;
                    for(i = 0;line[ol] != ';' && line[ol] != '\0'; ol++){
                        tc[i++] = line[ol];
                    }
                    tc[i] = '\0';
                    lsStrength = strDou(tc);
                }
                if (strcmp(option,"BackgroundDarkness") == 0) {
                    char tc [6];
                    for (int i = 0; i < 6; i++){
                        tc[i] = 0;
                    }
                    ol++;
                    int i;
                    for(i = 0;line[ol] != ';' && line[ol] != '\0'; ol++){
                        tc[i++] = line[ol];
                    }
                    tc[i] = '\0';
                    double bdark = strDou(tc);
                    background.r = bdark;
                    background.g = bdark;
                    background.b = bdark;
                }
                if (strcmp(option,"MaxReflection") == 0) {
                    char tc [6];
                    for (int i = 0; i < 6; i++){
                        tc[i] = 0;
                    }
                    ol++;
                    int i;
                    for(i = 0; line[ol] != ';' && line[ol] != '\0'; ol++){
                        tc[i++] = line[ol];
                    }
                    tc[i] = '\0';
                    maxRayReflect = atoi(tc);
                }
                if (strcmp(option,"MaxMovementSpeed") == 0) {
                    char tc [6];
                    for (int i = 0; i < 6; i++){
                        tc[i] = 0;
                    }
                    ol++;
                    int i;
                    for(i = 0;line[ol] != ';' && line[ol] != '\0'; ol++){
                        tc[i++] = line[ol];
                    }
                    tc[i] = '\0';
                    maxSpeed = strDou(tc);
                }
                if (strcmp(option,"MaxRotationSpeed") == 0) {
                    char tc [6];
                    for (int i = 0; i < 6; i++){
                        tc[i] = 0;
                    }
                    ol++;
                    int i;
                    for(i = 0;line[ol] != ';' && line[ol] != '\0'; ol++){
                        tc[i++] = line[ol];
                    }
                    tc[i] = '\0';
                    rotSpeed = strDou(tc);
                }
                if (strcmp(option,"DefaultStartingPos") == 0) {
                    char tc [120];
                    ol++;
                    int i;
                    for(i = 0;line[ol] != ';' && line[ol] != '&' && line[ol] != '\0'; ol++){
                        tc[i++] = line[ol];
                    }
                    tc[i] = '\0';
                    startingMasterRay.p = strCoor(tc);
                    if (line[ol] == '&'){
                        ol++;
                        for(i = 0;line[ol] != ';' && line[ol] != '\0'; ol++){
                            tc[i++] = line[ol];
                        }
                        tc[i] = '\0';
                        struct point p = strCoor(tc);
                        startingMasterRay.v.A = p.x;
                        startingMasterRay.v.B = p.y;
                        startingMasterRay.v.C = p.z;
                    } else {
                        exit(0);
                    }
                }
            }
}

double evalFunctAt(struct function f,double t){
		int garbage = 0;
		double result = recursiveParentheses(f.expression,t,&garbage);
		return result;
}

struct point evalFunction3d(struct function3d f, double t){
		double x = evalFunctAt(f.x,t);
		double y = evalFunctAt(f.y,t);
		double z = evalFunctAt(f.z,t);
		struct point p;
		p.x = x;
		p.y = y;
		p.z = z;
		return p;
   /* struct point p;
    p.x = 0;
    p.y = 0;
    p.z = 0;
    return p;*/
}

int isNum(char a){
		if (a >= 48 && a <= 57){
			return 1;
		} else {
			return 0;
		}
}

double recursiveParentheses(char af[],double t, int* index){
		int len = strlen(af);
		char numbuff[200];
		double numcurr = 0;
		int numbuffindex = 0;
    char operation = 'f';
    int nextsin = 0;
    int nextcos = 0;
		for (int i = 0; i <= len; i++){
        (*index)++;
				if (isNum(af[i]) || af[i] == '.'){
						numbuff[numbuffindex++] = af[i];
				} else {
            char ic = af[i];
            double numtmp = 0;
						if (numbuffindex != 0){
								numbuff[numbuffindex] = '\0';
                numtmp = strDou(numbuff);
								numbuffindex = 0;
						} else if (af[i] == '('){
                int jump = 0;
                numtmp = recursiveParentheses(&af[i+1],t,&jump);
                i += jump + 1;
                (*index) += jump + 1;
            }
            if (ic == 't'){
								numtmp = t;
                if (operation == 'f'){
                    operation = 't';
                    numcurr = numtmp;
                }
						}
            if (nextsin){
                numtmp = sin(numtmp);
                nextsin = 0;
            }
            if (nextcos){
                numtmp = cos(numtmp);
                nextcos = 0;
            }
            if (af[i] == 's'){
                nextsin = 1;
            }
            if (af[i] == 'c'){
                nextcos = 1;
            }
            if (af[i] != 's' && af[i] != 'c'){
                if (operation == 'f'){
                    numcurr = numtmp;
                }
                if (operation == '+'){
                    numcurr += numtmp;
                }
                if (operation == '*'){
                    numcurr = numcurr * numtmp;
                }
                if (operation == '-'){
                    numcurr -= numtmp;
                }
                if (operation == '/'){
                    numcurr = numcurr / numtmp;
                }
                if (operation == '^'){
                    numcurr = pow(numcurr,numtmp);
                }
            }


            if (af[i] == '+' || af[i] == '-' || af[i] == '*' || af[i] == '/' || af[i] == '^' ){
                operation = af[i];
            }
            if (af[i] == ')'){
              return numcurr;
            }
            if (af[i] == '\0'){
                return numcurr;
            }
				}
		}

		return 0;
}

void calcMovements(){
    for (int i = 0; i < o.scount;i++){
        struct point np = evalFunction3d(o.s[i].fv,gt);
        o.s[i].p = np;
    }


}

void init(){ //inizializálás: fájlok beolvasása, tömbök nullázása, dinamikus memória lefoglalása, stb
    o.scount = 0;
    o.plcount = 0;
    o.fplcount = 0;
    startingMasterRay.p.x = -20;
    startingMasterRay.p.y = 0;
    startingMasterRay.p.z = 0;
    startingMasterRay.v.A = 1;
    startingMasterRay.v.B = 0.1;
    startingMasterRay.v.C = 0.1; //beállítjuk egy értékre a vezérsugarat, hogy konfigurációs fájl nélkül is működjön a program
    FILE *f;
    f = fopen("RayTracing.conf","r");
    if (f == NULL) printf("Warning: cannot open configuration file (RayTracing.conf). Maybe it does not exist in the current directory. Using the default settings\n");
    else {
        char line[100];
        while (fgets(line,100,f) != NULL){
            configure(line);
        }
    }
    fclose(f);
    f = fopen(mapFile,"r");
    if (f == NULL) {
        printf("Error while opening the specified map file. Maybe it does not exist. \n");
        exit(0);
    } else {
        char line[200];
        int linecnt = 0;
        struct sphere spheres[50];
        struct plane planes[50];
        struct finitePlane fplanes[50];
        while (fgets(line,200,f) != NULL){
            linecnt++;
            char option[50];
            int ol = 0;
            for (; line[ol] != ':' && line[ol] != '\0';ol++){
                option[ol] = line[ol];
            }
            option[ol] = '\0';
            ol++;
            bool obj = false;
            if (strcmp(option,"Sphere") == 0){
                obj = true;
                char tc [60];
                for (int i = 0; i < 60; i++){
                    tc[i] = 0;
                }
                int i;
                for(i = 0;line[ol] != '=' && line[ol] != '\0'; ol++){
                    tc[i++] = line[ol];
                }
                tc[i] = '\0';
                spheres[o.scount].fv = strFunc3d(tc);
                ol++;
                for(i = 0;line[ol] != ';' && line[ol] != '|' && line[ol] != '\0';ol++) {
                    tc[i++] = line[ol];
                }
                tc[i] = '\0';
                spheres[o.scount].r = strDou(tc);
                if (line[ol] == '|') spheres[o.scount].isMirror = true; else spheres[o.scount].isMirror = false;
                o.scount++;
            }
            if (strcmp(option,"Plane") == 0){
                obj = true;
                char tc [60];
                for (int i = 0; i < 60; i++){
                    tc[i] = 0;
                }
                int i;
                for(i = 0;line[ol] != '=' && line[ol] != '\0'; ol++){
                    tc[i++] = line[ol];
                }
                tc[i] = '\0';
                planes[o.plcount].p = strCoor(tc);
                ol = ol + 1;
                for(i = 0;line[ol] != ';' && line[ol] != '\0';ol++) {
                    tc[i++] = line[ol];
                }
                tc[i] = '\0';
                struct point p = strCoor(tc);
                planes[o.plcount].v.A = p.x;
                planes[o.plcount].v.B = p.y;
                planes[o.plcount].v.C = p.z;
                o.plcount++;
            }
            if (strcmp(option,"FinitePlane") == 0){
                obj = true;
                char tc [100];
                for (int i = 0; i < 100; i++){
                    tc[i] = 0;
                }
                int i;
                for(i = 0;line[ol] != '=' && line[ol] != '\0'; ol++){
                    tc[i++] = line[ol];
                }
                tc[i] = '\0';
                fplanes[o.fplcount].p = strCoor(tc);
                ol++;
                for(i = 0;line[ol] != '&' && line[ol] != '\0'; ol++){
                    tc[i++] = line[ol];
                }
                tc[i] = '\0';
                struct point p = strCoor(tc);
                fplanes[o.fplcount].v1.A = p.x;
                fplanes[o.fplcount].v1.B = p.y;
                fplanes[o.fplcount].v1.C = p.z;
                ol++;
                for(i = 0;line[ol] != ';' && line[ol] != '\0' && line[ol] != '|';ol++) {
                    tc[i++] = line[ol];
                }
                tc[i] = '\0';
                p = strCoor(tc);
                fplanes[o.fplcount].v2.A = p.x;
                fplanes[o.fplcount].v2.B = p.y;
                fplanes[o.fplcount].v2.C = p.z;
                fplanes[o.fplcount].pl.v = crossProduct(fplanes[o.fplcount].v1,fplanes[o.fplcount].v2);
                fplanes[o.fplcount].pl.p = fplanes[o.fplcount].p;
                if (line[ol] == '|') fplanes[o.fplcount].pl.isMirror = true; else fplanes[o.fplcount].pl.isMirror = false;
                o.fplcount++;
            }
            if (obj == false) configure(line);
        }
        o.s = malloc(sizeof(struct sphere)*o.scount);
        o.pl = malloc(sizeof(struct plane)*o.plcount);
        o.fpl = malloc(sizeof(struct finitePlane)*o.fplcount);
        for (int i = 0; i < o.scount; i++){
            o.s[i] = spheres[i];
        }
        for (int i = 0; i < o.plcount; i++){
            o.pl[i] = planes[i];
        }
        for (int i = 0; i < o.fplcount; i++){
            o.fpl[i] = fplanes[i];
        }
        fclose(f);
    }
}

//szálak
void *rayTracingThread(void* arg){ //ez végzi a ray tracinget
    struct rtArgs r = *((struct rtArgs*)arg);
    free(arg); //a megkapott paramétert beolvassuk, és felszabadítjuk
    int j = r.pos;
    for (int i = 0; i < height; i++){
        do {
            struct rayColl rc = engage(pe.r[i][j]); //a perspektíva összes sugarát lekövetjük
            if (rc.h) { //ha ütközött valamivel a sugár
                double lsAngle = calcLSAngle(rc.p); //fényforrás szögét kiszámljuk ebben a pontban
                if (rc.type == 3) { //ha véges síkkal ütközött
                    struct finitePlane antiglare = *((struct finitePlane*) rc.hitObj);
                    if (!((vAngle(antiglare.pl.v,pe.r[i][j].v) <= M_PI/2 && vAngle(antiglare.pl.v,pToV(lightSource,rc.p)) <= M_PI/2) || (vAngle(antiglare.pl.v,pe.r[i][j].v) > M_PI/2 && vAngle(antiglare.pl.v,pToV(lightSource,rc.p)) > M_PI/2))){ //és ha a sík olyan oldalán vagyunk, ami ellentétes azzal az oldallal, ahol a fényforrás van
                        lsAngle = 0; //akkor ide a fényforrásból nem juthat fény (ez meggátolja, hogy bizonyos fényjelenségek (pl árnyékok) átlátszanak a síkokon
                    }
                }
                if (rc.type == 2) { //ha síkkal ütközött
                    struct plane antiglare = *((struct plane*) rc.hitObj);
                    if (!((vAngle(antiglare.v,pe.r[i][j].v) <= M_PI/2 && vAngle(antiglare.v,pToV(lightSource,rc.p)) <= M_PI/2) || (vAngle(antiglare.v,pe.r[i][j].v) > M_PI/2 && vAngle(antiglare.v,pToV(lightSource,rc.p)) > M_PI/2))){ //hasonlóan mint előbb..
                        lsAngle = 0;
                    }
                }
                double lightMod = ambientLight*(2*fabs(rc.angle)/M_PI)*255+(255-ambientLight*(2*fabs(rc.angle)/M_PI)*255)*lsStrength*(lsAngle/M_PI); //kiszámoljuk az adott pont intenzitását valami matematikailag nagyon hibás, de egyszerű és látványos formulával, amit próbálgatással találtam ki, és nem vagyok rá büszke
                struct pixel pi = {lightMod,lightMod,lightMod,255}; //pixel színének beállítása
                pixels[i][j] = pi; //pixel eltárolása a bufferben
            } else {
                pixels[i][j] = background; //ha nem ütközött a sugár, a pixel színe a háttér színével egyezik meg
            }
            j = j+r.jump; //lépünk a következő lekövetendő sugárra
        } while (j < width);
        j = j-width; //ez folyamatosítja a sorok közötti lépkedést: ha túlcsordulunk az egyik sorban, akkor a következő elején jövünk vissza
    }
}
void *perspectiveCalcThread(void* arg){ //perspektíva kíszámítását végző szál
    calcPersp(*((struct ray*)arg));
}
void *subThread(void* arg){ //ez a szál a nevével ellentétben gyakorlatilag mindent IS csinál, a felhasználói bemenet beolvasásán kívül
    SDL_Init(SDL_INIT_EVERYTHING);
 	SDL_Window *window = SDL_CreateWindow("Ray Tracing", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, 0);
	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    SDL_SetRenderDrawColor(renderer, background.r, background.g, background.b, background.a);
	SDL_RenderClear(renderer);
	SDL_RenderPresent(renderer); //SDL ablak setup
    startingMasterRay.p = transloc(startingMasterRay.p,vMult(reduceTo(startingMasterRay.v,distance),-1)); //vezérpont kiszámolása, itt csak eltoljuk a bementi pontot a vezérsugár irányában distance-el, hogy a vezérsík legyen a bementi koordináták helyén, és ne a vezérpont
    struct ray r = startingMasterRay;
    pthread_t *threadid = malloc(sizeof(pthread_t)*tcount); //annyi szál ID memória lefoglalása, amennyi kell (ezek lesznek a ray tracing szálak)
    pthread_t perspTid; //perspektíva kiszámolására szolgáló szál ID
    pthread_create(&perspTid,NULL,perspectiveCalcThread,&r); //elkezdjük kiszámolni a perspektívát
    double avgfps[5]; //átlagos fps kiszámolására szolgáló értékek
    for (int i = 0; i < 5; i++){
        avgfps[i] = 0;
    } //tömb lenullázása
    int fpscntr = 0;
    struct timeval ct; //idő lekérésere szolgáló struct
    gettimeofday(&ct,NULL);
    long peTi = ct.tv_sec * (int)1e6 + ct.tv_usec; //perspektíva ideje
		gt_zero = peTi/1000;
    long reTi; //minden más ideje
    calcMovements();
    do {
        reTi = ct.tv_sec * (int)1e6 + ct.tv_usec;
				gt = reTi/1000-gt_zero;
        pthread_join(perspTid,NULL); //bevárjuk a perspektívát
        for (int i = 0; i < tcount; i++) { //itt elindítjuk az összes ray tracing (továbbiakban csak rt) szálat
            struct rtArgs rtm;
            rtm.pos = i;
            rtm.jump = tcount;
            struct rtArgs* rt = malloc(sizeof(struct rtArgs));
            *rt = rtm;
            pthread_create(&threadid[i],NULL,rayTracingThread,rt);
        }
        gettimeofday(&ct,NULL);
        long t = ct.tv_sec * (int)1e6 + ct.tv_usec;
				gt = t/1000 - gt_zero;
        long tmp = peTi;
        peTi = t;
        t = t-tmp;
        for (int i = 0; i < tcount; i++) {
            pthread_join(threadid[i],NULL);
        } //bevárjuk az rt szálakat
        pthread_create(&perspTid,NULL,perspectiveCalcThread,&r); //ismét elkezdjük a perspektíva kiszámolását
        for (int i = 0; i < height; i++) { //kiírjuk a renderer-re a pixeleket
            for (int j = 0; j < width; j++) {
                if(pixels[i][j].a != 0) pixelRGBA(renderer,j,i,pixels[i][j].r,pixels[i][j].g,pixels[i][j].b,pixels[i][j].a);
            }
        }
        double fpssum = 0;
        for (int i = 0; i < 5; i++) {
            fpssum = fpssum + avgfps[i];
        }
        if (showFPS){ //fps kiírása
            char fps[317];
            sprintf(fps,"%lg",fpssum/5);
            fps[6] = ' ';
            fps[7] = 'F';
            fps[8] = 'P';
            fps[9] = 'S';
            fps[10] = '\0';
            stringRGBA(renderer, 5, 5, fps, 255, 0, 0, 255);
        }
        if (showPos) { //pozíció kiírása (hosszú és nem részletezném, nem a program szerves része)
            struct point pTP = transloc(r.p,reduceTo(r.v,distance));
            stringRGBA(renderer,width-140,5,"Position:",255,0,0,255);
            char pos[317];
            char copy[320];
            int i;
            int j;
            sprintf(pos,"%lg",pTP.x);
            j = -1;
            do {
                j++;
                copy[j+3] = pos[j];
            } while (pos[j] != '\0');
            for (i = 0; copy[i] != '.' && copy[i] != '\0'; i++){}
            copy[0] = 'X';
            copy[1] = ':';
            copy[2] = ' ';
            if (copy[i] == '.'){
                copy[i+3] = '\0';
            }
            stringRGBA(renderer,width-100,20,copy,255,0,0,255);
            sprintf(pos,"%lg",pTP.y);
            j = -1;
            do {
                j++;
                copy[j+3] = pos[j];
            } while (pos[j] != '\0');
            for (i = 0; copy[i] != '.' && copy[i] != '\0'; i++){}
            copy[0] = 'Y';
            copy[1] = ':';
            copy[2] = ' ';
            if (copy[i] == '.'){
                copy[i+3] = '\0';
            }
            stringRGBA(renderer,width-100,35,copy,255,0,0,255);
            sprintf(pos,"%lg",pTP.z);
            j = -1;
            do {
                j++;
                copy[j+3] = pos[j];
            } while (pos[j] != '\0');
            for (i = 0; copy[i] != '.' && copy[i] != '\0'; i++){}
            copy[0] = 'Z';
            copy[1] = ':';
            copy[2] = ' ';
            if (copy[i] == '.'){
                copy[i+3] = '\0';
            }
            stringRGBA(renderer,width-100,50,copy,255,0,0,255);
            sprintf(pos,"%lg",r.v.A);
            j = -1;
            do {
                j++;
                copy[j+3] = pos[j];
            } while (pos[j] != '\0');
            for (i = 0; copy[i] != '.' && copy[i] != '\0'; i++){}
            copy[0] = 'A';
            copy[1] = ':';
            copy[2] = ' ';
            if (copy[i] == '.'){
                copy[i+3] = '\0';
            }
            stringRGBA(renderer,width-100,65,copy,255,0,0,255);
            sprintf(pos,"%lg",r.v.B);
            j = -1;
            do {
                j++;
                copy[j+3] = pos[j];
            } while (pos[j] != '\0');
            for (i = 0; copy[i] != '.' && copy[i] != '\0'; i++){}
            copy[0] = 'B';
            copy[1] = ':';
            copy[2] = ' ';
            if (copy[i] == '.'){
                copy[i+3] = '\0';
            }
            stringRGBA(renderer,width-100,80,copy,255,0,0,255);
            sprintf(pos,"%lg",r.v.C);
            j = -1;
            do {
                j++;
                copy[j+3] = pos[j];
            } while (pos[j] != '\0');
            for (i = 0; copy[i] != '.' && copy[i] != '\0'; i++){}
            copy[0] = 'C';
            copy[1] = ':';
            copy[2] = ' ';
            if (copy[i] == '.'){
                copy[i+3] = '\0';
            }
            stringRGBA(renderer,width-100,95,copy,255,0,0,255);
        }
        SDL_RenderPresent(renderer);//renderer kirajzolása
        pthread_join(perspTid,NULL);
        calcMovements();
        gettimeofday(&ct,NULL);
        t = ct.tv_sec * (int)1e6 + ct.tv_usec - reTi;
        double persec = (double)t/1000000;
        avgfps[fpscntr++] = 1/persec;
        //innen kezdődnek a perspektíva számításai
        //előre-hátra haladás, W S gombokkal
        struct vector reduced = r.v;
        reduced.C = 0; //Ezt a sort kikommentelve az előre haladás követni fogja a vezérsugarat, de így nem tudunk a W vagy az S gombokkal Z irányban elmozdulni (szerintem ez egy jobb írányítási módszer)
        reduced = reduceTo(reduced,currentSpeed.B*persec);
        r.p = transloc(r.p,reduced); //a vezérpontot a jelenlegi sebességgel eltoljuk sugárirányba
        //jobbra-balra forgás, A D gombokkal
        struct vector zt = {0,0,1}; //z tengely
        struct vector animalCrossing = crossProduct(r.v,zt); //keresünk egy sugárra és z tengelyre is merőleges vektort
        if (isNullV(animalCrossing)) {
            r.v.A = 0.0000001; //Ha az animalCrossing vektor nullvektor, az azt jelenti, hogy a perspektíva vezérsugara párhuzamos a z tengellyel. Ez azonban matematikailag aluldefiniált állapot (a perspektíva állhat bármerre), és "természetes módon" szinte soha nem fordul elő. Ezért ilyenkor az A vektort 0tól különböző, kicsi értékre állítom be. Abban a ritka esetben, amikor ez a program futása közben megtörténik, eredményezheti a perspektíva elfordulását, de kiküszöböli azt a sokkal gyakoribb hibát, hogy a felhasználó teljesen felfelé mutató irányvektort ad meg kiindulópontnak.
            animalCrossing = crossProduct(r.v,zt);
        }
        animalCrossing.C = 0; //Elméletileg nem lehet 0tól különböző, mivel merőleges a z tengelyre, de ha valami balul üt ki akkor ez még jól jöhet
        reduced = reduceTo(animalCrossing,currentSpeed.A*persec); //Kiszámoljuk a sugárirányra merőleges eltolás irányvektorát
        r.p = transloc(r.p,reduced); //eltoljuk a vezérpontot ezzel
        //fel-le mozgás, Space és LShift gombokkal
        r.p.z = r.p.z + currentSpeed.C*persec; //ez csak egy egyszerű Z irányú mozgatás, itt nem kell "vektorozgatni"
        //Jobbra-balra forgás, nyilakkal
        double xyLength = sqrt(r.v.A*r.v.A+r.v.B*r.v.B); //a vezérsugár xy síkra való vetületének a hossza
        double xangle; //Az x tengellyel bezárt szöge a vezérsugár xy képének
        if (r.v.B > 0) xangle = v2DAngle(r.v.A,r.v.B,1,0); else xangle = -1*v2DAngle(r.v.A,r.v.B,1,0); //itt ezt kiszámoljuk, xangle-t negatív tartományban értelmezzük, ha a vezérsugár y irányban negatív értéket vesz fel (az xy koordinátarendszerben lefelé mutat a vezérsugár képe)
        double tangle = xangle + currRotSpeed.C * persec; //ez az új szög, ahogy a vektor a forgatás után állni fog
        r.v.A = xyLength * cos(tangle);
        r.v.B = xyLength * sin(tangle); //beállítjuk az új szög alapján az új irányvektort
        //fel-le forgás, nyilakkal - nem részletezem nagyon, hasonlóan történik mint a jobbra-balra
        double yzLength = sqrt(r.v.B*r.v.B+r.v.C*r.v.C);
        struct vector xyvector = r.v;
        xyvector.C = 0;
        xangle = vAngle(zt,r.v);
        tangle = xangle - currRotSpeed.A * persec;
        if (tangle <= 0) tangle = 0.00000001;
        if (tangle >= M_PI) tangle = M_PI-0.00000001; //Ez az ellen van, hogy ne lehessen átfordulni akkor, ha valaki mondjuk a felfele gombbal "túl sokat néz felfelé" (vagy lefelé), így megakad a forgás amikor elérte ezt
        if (yzLength == 0) yzLength = 0.000001; //nem lehet nulla, és ezt sokkal egyszerűbb így kiküszöbölni mint matematikailag pontosan levezetni
        xyvector = reduceTo(xyvector,yzLength * sin(tangle));
        r.v.A = xyvector.A;
        r.v.B = xyvector.B;
        r.v.C = yzLength * cos(tangle);
        r.v = reduceTo(r.v,1); //matematikailag helyes lenne, ha az irányvektor hossza nincs 1re beállítva, azonban így a vektor hossza folyamatosan csökkenne, és a double típus határai miatt nagyjából 2 forduló alatt ténylegesen 0 lenne.
//        lightSource.x = lightSource.x - 25*(double)t/1000000;
//        lightSource.y = lightSource.y + 25*(double)t/1000000;
//        lightSource.z = lightSource.z - 25*(double)t/1000000;
//        printf("%lf fps\n",1/((double)t/1000000));
        if (reset) { //alaphelyzetbe állítás, R gombbal
            r = startingMasterRay;
            reset = false;
        }
        if (fpscntr == 5) fpscntr = 0; //ez nem számláló a nevével ellentétben, inkább egy mutató az fps tömb aktuálisan kicserélendő elemére
    } while(!sigterm);
    pthread_join(perspTid,NULL); //bevárjuk a perspektíva számoló szálat
    free(threadid); //felszabadítjuk az rt szálaknak lefoglalt memóriát
}

void *eventThread(void* arg){
	SDL_Event ev;
	while (!sigterm){
			while(SDL_PollEvent(&ev)){
					if (ev.type == SDL_KEYDOWN){ //itt történik a billentyűzet eventek kezelése
							switch (ev.key.keysym.sym) {
									case SDLK_d:
											currentSpeed.A = maxSpeed;
											break;
									case SDLK_a:
											currentSpeed.A = -1*maxSpeed;
											break;
									case SDLK_w:
											currentSpeed.B = maxSpeed;
											break;
									case SDLK_s:
											currentSpeed.B = -1*maxSpeed;
											break;
									case SDLK_SPACE:
											currentSpeed.C = maxSpeed;
											break;
									case SDLK_LSHIFT:
											currentSpeed.C = -1*maxSpeed;
											break;
									case SDLK_LEFT:
											currRotSpeed.C = rotSpeed;
											break;
									case SDLK_RIGHT:
											currRotSpeed.C = -1*rotSpeed;
											break;
									case SDLK_UP:
											currRotSpeed.A = rotSpeed;
											break;
									case SDLK_DOWN:
											currRotSpeed.A = -1*rotSpeed;
											break;
									//nem fontos funkciók
									case SDLK_f:
											if (showFPS) showFPS = false; else showFPS = true;
											break;
									case SDLK_r:
											reset = true;
											break;
									case SDLK_p:
											if (showPos) showPos = false; else showPos = true;
											break;
							}
					}
					if (ev.type == SDL_KEYUP){
							switch (ev.key.keysym.sym) {
									case SDLK_d:
											currentSpeed.A = 0;
											break;
									case SDLK_a:
											currentSpeed.A = 0;
											break;
									case SDLK_w:
											currentSpeed.B = 0;
											break;
									case SDLK_s:
											currentSpeed.B = 0;
											break;
									case SDLK_SPACE:
											currentSpeed.C = 0;
											break;
									case SDLK_LSHIFT:
											currentSpeed.C = 0;
											break;
									case SDLK_LEFT:
											currRotSpeed.C = 0;
											break;
									case SDLK_RIGHT:
											currRotSpeed.C = 0;
											break;
									case SDLK_UP:
											currRotSpeed.A = 0;
											break;
									case SDLK_DOWN:
											currRotSpeed.A = 0;
											break;
							}
					}
					if (ev.type == SDL_QUIT) {
							sigterm = true;
					}
			}
			sleep(1/pollingRate);
	}
}

int main(int argc, char **arg){ //TODO stabil windows build //TODO objektumok mozgása (Ide írtam azokat a dolgokat, amiket még meg kéne csinálni, ezt a kettőt már nem csináltam meg [a win build nem sikerült sok próbálkozás után, nagyon át kéne írni hozzá sok dolgot, pl szálakat, idő lekérést, stb, az objektumok mozgását pedig már nem volt kedvem megcsinálni{pedig nem lenne sok munka}])
    if (argc >= 2) mapFile = arg[1]; else {
        printf("Error: No map file specified\n"); //map nélkül nem tud elindulni a program
        exit(0);
    }
    init(); //inicializáljuk a programot (file kezelés, változók beállítása)
    pthread_t threadid; //subthread szála
    pthread_create(&threadid,NULL,subThread,NULL);
		pthread_t eventhread;
		pthread_create(&eventhread,NULL,eventThread,NULL);

		pthread_join(eventhread,NULL);
		pthread_join(threadid,NULL);

		free(pe.r);
		free(o.s);
		free(o.pl);
		free(o.fpl);
		free(pixels); //néhány dinamikusan lefoglalt változó felszabadítása
		SDL_Quit();
		exit(0);
		return 0;
}
struct sphereColl raySphereCollision(struct ray ray, struct sphere sphere){ //gömb-sugár ütközés
    struct sphereColl m;
    struct point kp = sphere.p;
    double xg = kp.x; //g
    double yg = kp.y; //h
    double zg = kp.z; //j
    double r = sphere.r;
    double xn = ray.p.x; //c
    double yn = ray.p.y; //v
    double zn = ray.p.z; //b
    double A = ray.v.A;
    double B = ray.v.B;
    double C = ray.v.C; //ezekre csak az egyszerűség kedvéért volt szükség
    if (A == 0 && B == 0 && C == 0){
        m.h=false; //ha nullvektort kaptunk, akkor nincs ütközés
        return m;
    }
    double d;
    double m1;
    double m2;
	if (fabs(A) > 0.0000000001) { //erre azért van szükség, mert egy double típus sosem lesz szinte pontosan 0 egy számítási sor után
        d = (pow((-2)*A*A*xg+2*A*B*yn-2*A*B*yg-2*A*C*zg+2*A*C*zn-2*B*B*xn-2*C*C*xn,2)-4*(A*A+B*B+C*C)*(A*A*xg*xg+A*A*yg*yg-2*A*A*yg*yn+A*A*zg*zg-2*A*A*zg*zn+A*A*yn*yn+A*A*zn*zn-A*A*r*r+2*A*B*yg*xn-2*A*B*xn*yn+2*A*C*zg*xn-2*A*C*xn*zn+B*B*xn*xn+C*C*xn*xn)); //ez a matematikai összefüggés visszaadja a metszéspontok kiszámításának másodfokú egyenlete diszkriminánsát
        if (d < 0) { //ha a diszkrimináns negatív, nincs metszéspont
            m.h=false;
            return m;
        }
        m1 = (1/(A*A+B*B+C*C))*(sqrt(d)/2+A*A*xg+A*B*yg-A*B*yn+A*C*zg-A*C*zn+B*B*xn+C*C*xn); //első metszéspont x koordinátája
        m2 = (1/(A*A+B*B+C*C))*(sqrt(d)/(-2)+A*A*xg+A*B*yg-A*B*yn+A*C*zg-A*C*zn+B*B*xn+C*C*xn); //a második metszéspont x koordinátája
        m.p1.x = m1;
        m.p1.y = (m1-xn)*B/A+yn;
        m.p1.z = (m1-xn)*C/A+zn;
        m.p2.x = m2;
        m.p2.y = (m2-xn)*B/A+yn;
        m.p2.z = (m2-xn)*C/A+zn; //itt a többi koordináta kiszámítása az egyenes egyenletéből kijön
    }
    else {
        if (B != 0) { //sajnos ha az irányvektor x komponense nulla, egy másik összefüggést kell alkalmaznunk, de minden más ugyan olyan, mint az előbb
            d = (-1)*B*B*(zn*zn*B*B-2*zn*B*B*zg+2*zn*B*C*yg-2*zn*B*C*yn+B*B*xn*xn-2*B*B*xn*xg+B*B*xg*xg+B*B*zg*zg-B*B*r*r-2*B*C*yg*zg+2*B*C*zg*yn+xn*xn*C*C-2*xn*C*C*xg+C*C*xg*xg+C*C*yg*yg-2*C*C*yg*yn-C*C*r*r+C*C*yn*yn);
            if (d < 0) {
                m.h=false;
                return m;
            }
            m1 = (1/(B*B+C*C))*(sqrt(d)-zn*B*C+B*B*yg+B*C*zg+C*C*yn);
            m2 = (1/(B*B+C*C))*(-1*sqrt(d)-zn*B*C+B*B*yg+B*C*zg+C*C*yn);
            m.p1.x = xn;
            m.p1.y = m1;
            m.p1.z = ((m.p1.y-yn)*C/B)+zn;
            m.p2.x = xn;
            m.p2.y = m2;
            m.p2.z = ((m.p2.y-yn)*C/B)+zn;
        }
        else { //és megint mást ha az A és a B is nulla
                d = (-1)*xn*xn+2*xn*xg-xg*xg-yg*yg+2*yg*yn+r*r-yn*yn;
                if (d < 0) {
                    m.h=false;
                    return m;
                }
                m1 = zg-sqrt(d);
                m2 = zg+sqrt(d);
                m.p1.x = xn;
                m.p1.y = yn;
                m.p1.z = m1;
                m.p2.x = xn;
                m.p2.y = yn;
                m.p2.z = m2;
        }
    }
    m.h = true; //ha ezek után még nem volt return, akkor biztosan történt ütközés
    if (pDist(ray.p,m.p1) > pDist(ray.p,m.p2)){ //p1-nek kell lennie a közelebbi pontnak a kapott sugár megadott pontjához képest, ha ez nem így van, akkor meg kell cserélni a két pontot
        double tmp;
        tmp = m.p1.x;
        m.p1.x = m.p2.x;
        m.p2.x = tmp;
        tmp = m.p1.y;
        m.p1.y = m.p2.y;
        m.p2.y = tmp;
        tmp = m.p1.z;
        m.p1.z = m.p2.z;
        m.p2.z = tmp;
    }
    struct vector radius = pToV(kp,m.p1); //a gömb középpontjától az ütközési pontba mutató vektor
    struct vector corsa = crossProduct(radius,ray.v); //egy a sugárra és a radius vektorra merőleges vektor
    struct vector crossa = crossProduct(corsa,radius); //ez pedig már az érintő egyenes vektora az ütközési pontban
    m.angle = vAngle(crossa,ray.v); //az egyenes és a gömb felülete által bezárt szög
    return m;
}
struct rayColl plColl(struct ray r, struct plane pl){ //sík-sugár ütközés
    struct rayColl rc;
    if (isNullV(crossProduct(r.v,pl.v))) { //nullvektor esetén nincs ütközés
        rc.h = false;
        return rc;
    }
    double a = r.v.A;
    double b = r.v.B;
    double c = r.v.C;
    double A = pl.v.A;
    double B = pl.v.B;
    double C = pl.v.C;
    double s = pl.p.x;
    double t = pl.p.y;
    double u = pl.p.z;
    double n = r.p.x;
    double o = r.p.y;
    double p = r.p.z; //szintén csak az egyszerűség és az átláthatóság érdekében
    rc.p.x = (a*A*s-a*B*o+a*B*t-a*C*p+a*C*u+b*B*n+c*C*n)/(a*A+b*B+c*C); //ütközés x koordinátája
    if (fabs(rc.p.x) > 100000) { //ha túl nagy, akkor ott már nincs ütközés
        rc.h = false;
        return rc;
    }
    if (a == 0) {
        rc.p.y = (a*A*o-A*b*n+A*b*s+b*B*t-b*C*p+b*C*u+c*C*o)/(a*A+b*B+c*C); //y
        rc.p.z = (a*A*p-A*c*n+A*c*s+b*B*p-B*c*o+B*c*t+c*C*u)/(a*A+b*B+c*C); //z
    } else {
        rc.p.y = (rc.p.x-n)*b/a+o;
        rc.p.z = (rc.p.x-n)*c/a+p;
    }
    if (fabs(rc.p.y) > 100000) {
        rc.h = false;
        return rc;
    }
    if (fabs(rc.p.z) > 100000) {
        rc.h = false;
        return rc;
    }
    double pot = vAngle(r.v,pl.v); //sugár beesési szöge
    if (pot > M_PI/2) pot = vAngle(r.v,vMult(pl.v,-1)); //ha a sugár a sík "hátoldalára" (a sík normálvektorának irányával ellenkező irányban) akkor a sík normálvektorát megfordítjuk ehhez a számításhoz
    rc.angle = M_PI/2-pot; //a sugár és a sík szöge (90 fok - beesési szög)
    rc.h = true;
    return rc;
}
void calcPersp(struct ray r){ //perspektíva kiszámítása
/*    struct sphere dist;
    dist.p = r.p;
    dist.r = distance;
    struct sphereColl sc = raySphereCollision(r,dist);*/ // Eredeti megfontolásom ez volt, de a gömb-egyenes metszéspont kiszámítása nagyon erőforrás-igényes, a vektorműveletek "olcsóbbak" (és ez az ötlet egyébként hibás is volt, ugyanis ha egy adott ponton átmenő egynes elmetszi ugyan azon pont körüli gömböt, akkor mindkét irányban lesz metszéspont, ideális világban azonos távolságra. Azonban így a c nyelv kerekítéseire voltam bízva abban a kérdésben, hogy a perspektíva véletlenszerűen megfordul-e (adott bemeneti értékeknél persze mindig ugyan úgy állt), ezért még le kellett volna ellenőrizni, hogy melyik pont felé mutat az r ray irányvektora, ahogy azt az engage függvénynél is teszem)
    struct vector iv = reduceTo(r.v,distance); //ez a perspektíva középső pontjára mutató vektor a vezérpontból
    struct point masterp = transloc(r.p,iv); //ez a mesterpont, a perpektíva középső pontja
    struct vector masterx; //ebben egy, a sugárra és a z tengelyre merőleges, "jobbra" (a néző szemszögéből) mutató vektornak kell lennie
    struct vector zt = {0,0,1}; //z tengely
    if (r.v.C != 0 && r.v.A == 0 && r.v.B == 0) r.v.A = 0.001; //A vezérsugár nem állhat függőlegesen, mert ez aluldefiniálná a perspektívát: Nem tudjuk, hogy milyen irányba áll.
    masterx = crossProduct(r.v,zt);
    masterx = reduceTo(masterx,perspPixelSize); //masterx hosszát beállítjuk arra, amekkora távolságot ugrunk egy-egy pixellel
    struct vector masterz; //masterx-re és a sugárra merőleges, felfelé mutat
    masterz = crossProduct(masterx,r.v);
    masterz = reduceTo(masterz,perspPixelSize);
    struct vector startvv = vAdd(vMult(masterz,height/2),vMult(masterx,width/(-2))); //ez a perspektíva bal felső sarkába mutat a perspektíva közepéből
    struct point startp = transloc(masterp,startvv); //kezdő pont, ez van a bal felső sarokban
    struct vector startv = pToV(r.p,startp); //ez a kezdő vektor, a vezérpontból mutat a bal felső sarokba
    for (int i = 0; i < height; i++){
        for (int j = 0; j < width; j++){
            struct ray temp;
            temp.v = vAdd(startv,vAdd(vMult(masterx,j),vMult(masterz,-1*i))); //kiszámoljuk a perspektíva összes pontjába mutató vektort a vezérpontból
            temp.p = r.p;
            pe.r[i][j] = temp;
            pe.r[i][j].isLight = false;
            pe.r[i][j].reflected = 0;
        }
    }
}

struct rayColl engage (struct ray r){ //ez végzi élesben a sugarak ütköztetését az összes objektummal, ez számolja a tükröződéseket is
    struct rayColl min;
    min.p.x = 2000000; //beállítjuk egy nagy értékre, ami úgy sem fog kijönni számolásokból, és utána ennél kisebb értékeket keresünk (így a legegyszerűbb)
    min.h = false;
    struct ray mir; //erre akkor van szükség, ha tükröződik a sugár, és ez a visszatükrözött sugár
    for (int i = 0; i < o.scount;i++){ //gömbök ütközése
        struct sphereColl sc = raySphereCollision(r,o.s[i]);
        if (sc.h) { //ha történt gömbbel ütközés
            if (fabs(vAngle(pToV(r.p,sc.p1),r.v)) < 0.0000001) { //ha az ütközési pontba mutató vektor (a sugár pontjából) és a sugár irányvektora egy irányba mutat (tehát nincs a hátunk mögött a gömb)
                min.h = true; //megtörtént az ütközés
                    if (pDist(sc.p1,r.p) < pDist(min.p,r.p)) { //ha ez az eddig talált legközelebbi ütközési pont az adott sugárra
                        min.p = sc.p1;
                        min.angle = sc.angle;
                        min.type = 1;
                        min.isMirrored = o.s[i].isMirror;
                        min.hitObj = &o.s[i]; //átmásoljuk a sphereColl adatait min rayColl-ba, és adunk meg néhány extra adatot
                        if (o.s[i].isMirror) { //ha a gömb tükör
                                if(!r.isLight && sc.angle*180/M_PI > 15 && r.reflected <= maxRayReflect) { //ha 15 foknál nagyobb az ütközés, és nem fénysugár (fénysugár nem tud visszatükröződni), és még a fénysugarunk még kevesebbszer tükröződött mint a maximális érték
                                    struct ray mray;
                                    mray.p = sc.p1;
                                    struct point asdlol;
                                    asdlol.x = 0;
                                    asdlol.y = 0;
                                    asdlol.z = 0;
                                    struct vector mv = reduceTo(pToV(o.s[i].p,sc.p1),vAbs(r.v)*sin(sc.angle));
                                    struct vector addit = vMult(vAdd(mv,r.v),2);
                                    struct point wanderer = transloc(transloc(sc.p1,vMult(r.v,-1)),addit);
                                    mray.v = pToV(sc.p1,wanderer); //ez egy sima tükrözés a gömb felületének normálvektorára vektorműveletekkel
                                    mir = mray;
                                } else {
                                    min.isMirrored = false;
                                }
                        }
                    }
            }
        }
    }
    for (int i = 0; i < o.plcount;i++){ //síkok ütközése
        struct rayColl prc = plColl(r,o.pl[i]);
        if (prc.h){ //ha történt síkkal ütközés
            if (fabs(vAngle(pToV(r.p,prc.p),r.v)) < 0.0000001) {
                min.h = true;
                if (pDist(prc.p,r.p) < pDist(min.p,r.p)) { //gömbhöz hasonlóan, de síkról nem tud visszatükröződni sugár (bár a plane structak van isMirror eleme, de ez a finitePlane-eknél érdekes
                    min.type = 2;
                    min.p = prc.p;
                    min.angle = prc.angle;
                    min.isMirrored = false;
                    min.hitObj = &o.pl[i];
                }
            }
        }
    }
    for (int i = 0; i < o.fplcount; i++){ //véges síkok ütközése
        struct rayColl prc = plColl(r,o.fpl[i].pl);
        if (prc.h) {
            struct vector vt = pToV(o.fpl[i].p,prc.p);
            double ang = vAngle(o.fpl[i].v1,vt);
            double ang2 = vAngle(o.fpl[i].v2,vt);
            double vtabs = vAbs(vt);
            if (vtabs * sin(ang) <= vAbs(o.fpl[i].v2) && vtabs * cos(ang) <= vAbs(o.fpl[i].v1) && ang < M_PI/2 && ang2 < M_PI/2) { //kiszámoljuk a véges sík ütközését
                if (fabs(vAngle(pToV(r.p,prc.p),r.v)) < 0.0000001) { //innentől hasonlóan mint a gömböknél
                        min.h = true;
                        if (pDist(prc.p,r.p) < pDist(min.p,r.p)) {
                            min.type = 3;
                            min.p = prc.p;
                            min.angle = prc.angle;
                            min.isMirrored = o.fpl[i].pl.isMirror;
                            min.hitObj = &o.fpl[i];
                            if(o.fpl[i].pl.isMirror && !r.isLight && r.reflected <= maxRayReflect) {
                                struct ray mray;
                                mray.p = prc.p;
                                struct vector mv;
                                if (vAngle(o.fpl[i].pl.v,r.v) > M_PI/2) { //csak az egyik oldal lehet tükör
                                    mv = reduceTo(o.fpl[i].pl.v,vAbs(r.v)*sin(prc.angle));
                                } else {
                                    min.isMirrored = false;
                                }
                                struct vector addit = vMult(vAdd(mv,r.v),2);
                                struct point wanderer = transloc(transloc(prc.p,vMult(r.v,-1)),addit);
                                mray.v = pToV(prc.p,wanderer);
                                mir = mray;
                            } else {
                                min.isMirrored = false;
                            }
                        }
                }
            }
        }
    }
    if (min.isMirrored && min.h) { //ha tükröződött a sugár, és történt ütközés, akkor rekurzívan ezt újra eljátszuk a tükrözött sugárral
        mir.reflected = r.reflected+1;
        min = engage(mir);
    }
    return min;
}

double calcLSAngle(struct point p){ //egy adott pontban számolja ki a fényforrás és az ugyanebben a pontban lévő objektum által bezárt szöget
    struct vector v = pToV(lightSource,p);
    struct ray tr;
    tr.v = v;
    tr.p = lightSource;
    tr.isLight = true; //fénysugarat nem lehet tükrözni
    struct rayColl rc = engage(tr); //itt is az engage-t használom, így ezek a sugarak is elakadhatnak objektumokban
    if (!rc.h) return 0; //ha nincs ütközés, nincs szög
    if (fabs(rc.p.x - p.x) < 0.00005 && fabs(rc.p.y - p.y) < 0.00005 && fabs(rc.p.z - p.z) < 0.00005) { //ha az ütközési pont és a megadott pont (nagyjából) egyenlő
        return fabs(rc.angle);
    }
    return 0;
}
void printPoint(struct point p){ //egy pontot ír ki (csak debugra jó)
    printf("point: %lf %lf %lf\n",p.x,p.y,p.z);
}
void printVector(struct vector v){ //egy vektort ír ki (csak debugra jó)
    printf("vector: %lf %lf %lf\n",v.A,v.B,v.C);
}

struct vector crossProduct(struct vector v1, struct vector v2){ //vektoriális szorzat (egyéb indoklás nélkül)
    struct vector v;
    v.A = v1.B*v2.C-v1.C*v2.B;
    v.B = v1.C*v2.A-v1.A*v2.C;
    v.C = v1.A*v2.B-v1.B*v2.A;
    return v;
}
double vAngle(struct vector v1, struct vector v2){ //két vektor által bezárt szög
    double invangle = (v1.A*v2.A+v1.B*v2.B+v1.C*v2.C)/(vAbs(v1)*vAbs(v2)); //skaláris szorzatból számolva
    if (invangle > 1) invangle = 1; //néha a double típus határai miatt itt jelent meg 1-nél nagyobb vagy kisebb érték (matematikailag nem lehetne)
    if (invangle < -1) invangle = -1;
    return acos(invangle);
}
double v2DAngle(double v1A, double v1B, double v2A, double v2B){ //ugyan az mint 3D-re, csak kevesebb paraméterrel
    double invangle = (v1A*v2A+v1B*v2B)/(sqrt(v1A*v1A+v1B*v1B)*sqrt(v2A*v2A+v2B*v2B));
    if (invangle > 1) invangle = 1;
    if (invangle < -1) invangle = -1;
    return acos(invangle);
}
double vAbs(struct vector v){ //egy vektor hossza
    return sqrt(v.A*v.A+v.B*v.B+v.C*v.C);
}
struct vector vAdd(struct vector v1, struct vector v2){ //két vektor összege
    struct vector v;
    v.A = v1.A+v2.A;
    v.B = v1.B+v2.B;
    v.C = v1.C+v2.C;
    return v;
}
struct vector reduceTo(struct vector v, double targetLength){ //egy vektor hosszának beállítása az adott értékre, irányának megtartásával
    double lambda = sqrt(targetLength*targetLength/(v.A*v.A+v.B*v.B+v.C*v.C));
    v.A = v.A*lambda;
    v.B = v.B*lambda;
    v.C = v.C*lambda;
    if (targetLength < 0) v = vMult(v,-1);
    return v;
}
struct point vToP(struct vector v){ //vektor végpontja
    struct point p = {v.A,v.B,v.C};
}
struct vector vMult(struct vector v1, double lambda){ //vektor szorzása skalárral
    struct vector v;
    v.A = v1.A*lambda;
    v.B = v1.B*lambda;
    v.C = v1.C*lambda;
    return v;
}
struct point transloc(struct point p, struct vector v){ //pont eltolása vektorral
    struct point mp;
    mp.x = p.x+v.A;
    mp.y = p.y+v.B;
    mp.z = p.z+v.C;
    return mp;
}
struct vector pToV(struct point p1,struct point p2){ //két pont által meghatározott vektor
    //p2-be mutat
    struct vector v;
    v.A = p2.x-p1.x;
    v.B = p2.y-p1.y;
    v.C = p2.z-p1.z;
    return v;
}
double pDist(struct point p1, struct point p2){ //két pont távolsága
    return sqrt((p1.x-p2.x)*(p1.x-p2.x)+(p1.y-p2.y)*(p1.y-p2.y)+(p1.z-p2.z)*(p1.z-p2.z));
}
bool isNullV(struct vector v){ //null vektor-e
    if (v.A == 0 && v.B == 0 && v.C == 0) return true;
    return false;
}
double dotProduct(struct vector v1, struct vector v2){ //skaláris szorzat
    return v1.A*v2.A+v1.B*v2.B+v1.C*v2.C;
}
bool isSuspiciousV(struct vector v){ //inf vagy hasonló érték szerepel-e a vektorban (csak debug)
    if ((v.A == v.A)&&(v.B == v.B)&&(v.C == v.C)) return false;
    return true;
}
double strDou(char in[]){ //stringet konvertál double-be
    char tc [260];
    char tizc [260];
    int eg; //egész
    int tiz; //tizedes
    for (int i = 0; i < 50; i++){
        tc[i] = 0;
        tizc[i] = 0;
    }
    int ol = 0;
    for(; in[ol] != '.' && in[ol] != '\0'; ol++){
        tc[ol] = in[ol];
    }
    tc[ol] = '\0';
    eg = atoi(tc);
    if (in[ol] != '\0') ol++;
    int k;
    for(k = 0; in[ol] != '\0'; ol++){
        tizc[k++] = in[ol];
    }
    tizc[k+1] = '\0';
    tiz = atoi(tizc);
    double tizedes = (double) tiz;
    while (tizedes > 1) {
        tizedes = tizedes/10;
    }
    for (int i = 0; i < 6 && tizc[i] == '0'; i++){
        tizedes = tizedes/10;
    }
    if (tc[0] == '-') {
        if (eg == 0) return (-1)*tizedes;
        return eg-tizedes;
    }
    return eg+tizedes;
}
struct point strCoor(char in[]){ //(x,y,z) alakú stringet konvertál pontra
    struct point p;
    int j;
    char tc[260];
    for (int i = 0; i < 260; i++){
        tc[i] = 0;
    }
    if (in[0] == '('){
        for (j = 1; in[j] != '\0' && in[j] != ','; j++){
            tc[j-1] = in[j];
        }
        tc[j-1] = '\0';
        p.x = strDou(tc);
        if (in[j++] == ',') {
            int k;
            for (k = 0; in[j] != '\0' && in[j] != ','; j++){
                tc[k++] = in[j];
            }
            tc[k] = '\0';
            p.y = strDou(tc);
            if (in[j++] == ','){
                for (k = 0; in[j] != '\0' && in[j] != ')'; j++){
                    tc[k++] = in[j];
                }
                tc[k] = '\0';
                p.z = strDou(tc);
            }
        }
    }
    return p;
}

struct function3d strFunc3d(char in[]){ //{x(t),y(t),z(t)} alakú stringet konvertál 3d függvénnyé
	struct function3d result;

	int index = 0;
	int start = 0;
	while (in[index] != '{'){
		index++;
	}
	start = index;
	while (in[index] != ','){
		index++;
	}
	int len = index-start;
	char *x = (char*) malloc(len*sizeof(char));
	int ptr = 0;
	for (int i = start + 1; i < index; i++){
		x[ptr++] = in[i];
	}
	x[len-1] = '\0';

	index++;
	start = index;
	while (in[index] != ','){
		index++;
	}
	len = index-start+1;
	char *y = (char*) malloc(len*sizeof(char));
	ptr = 0;
	for (int i = start; i < index; i++){
		y[ptr++] = in[i];
	}
	y[len-1] = '\0';

	index++;
	start = index;
	while (in[index] != '}'){
		index++;
	}
	len = index-start+1;
	char *z = (char*) malloc(len*sizeof(char));
	ptr = 0;
	for (int i = start; i < index; i++){
		z[ptr++] = in[i];
	}
	z[len-1] = '\0';

	result.x.expression = x;
	result.y.expression = y;
	result.z.expression = z;

	return result;
}
