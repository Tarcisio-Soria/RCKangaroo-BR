// This file is a part of RCKangaroo software
// (c) 2024, RetiredCoder (RC)
// License: GPLv3, see "LICENSE.TXT" file
// https://github.com/RetiredC
//
// RCKangaroo BR - traducao PT-BR, tabela -puzzle e modo loteria (-rnd)
// Modificacoes (c) 2026 Tarcisio Soria (a.k.a KKND). Licenca: GPLv3 (mesma da base).


#include <iostream>
#include <vector>
#include <time.h>
#include "cuda_runtime.h"
#include "cuda.h"
#ifndef _WIN32
#include <nvml.h>
#define HAVE_NVML 1
#endif

#include "defs.h"
#include "utils.h"
#include "GpuKang.h"


EcJMP EcJumps1[JMP_CNT];
EcJMP EcJumps2[JMP_CNT];
EcJMP EcJumps3[JMP_CNT];

RCGpuKang* GpuKangs[MAX_GPU_CNT];
int GpuCnt;
volatile long ThrCnt;
volatile bool gSolved;

EcInt Int_HalfRange;
EcPoint Pnt_HalfRange;
EcPoint Pnt_NegHalfRange;
EcInt Int_TameOffset;
Ec ec;

CriticalSection csAddPoints;
u8* pPntList;
u8* pPntList2;
volatile int PntIndex;
TFastBase db;
EcPoint gPntToSolve;
EcInt gPrivKey;

volatile u64 TotalOps;
u32 TotalSolved;
u32 gTotalErrors;
u64 PntTotalOps;
bool IsBench;

u32 gDP;
u32 gRange;
EcInt gStart;
bool gStartSet;
EcPoint gPubKey;
u8 gGPUs_Mask[MAX_GPU_CNT];
char gTamesFileName[1024];
double gMax;
bool gGenMode; //tames generation mode
bool gIsOpsLimit;
bool gRndMode;  //modo randomico (loteria por janelas) - RCKangaroo BR
int gWBits;     //bits da janela sorteada no modo randomico - RCKangaroo BR
int gPuzzleNum = 0;        //numero do puzzle carregado (0 = nenhum) - p/ bloco de config
bool gPuzzleSolved = false;//puzzle carregado e resolvido (teste)?
bool gQuiet = false; //silencia o output verboso do solve (usado no modo loteria p/ painel fixo)
bool gPanelMode = false; //painel fixo no solve direto (mesmo visual do modo loteria) - RCKangaroo BR
static const char* ui_num(unsigned long long v); //fwd decl (helper de UI, definido antes de main)

// ===== Tabela de puzzles com chave publica conhecida - RCKangaroo BR =====
// range = N-1, start = 2^(N-1). Pubkeys dos resolvidos derivadas da privkey
// publicada (verificaveis); pubkey do #140 exposta on-chain.
struct PuzzleEntry { int num; int range; const char* start; const char* pubkey; int solved; };
static const PuzzleEntry gPuzzleTable[] = {
	//--- TESTES (resolvidos: chave conhecida, pubkey verificada - deriva o endereco) ---
	{  50,  49, "2000000000000", "03F46F41027BBF44FAFD6B059091B900DAD41E6845B2241DC3254C7CDD3C5A16C6", 1 },
	{  55,  54, "40000000000000", "0385A30D8413AF4F8F9E6312400F2D194FE14F02E719B24C3F83BF1FD233A8F963", 1 },
	{  60,  59, "800000000000000", "0348E843DC5B1BD246E6309B4924B81543D02B16C8083DF973A89CE2C7EB89A10D", 1 },
	{  65,  64, "10000000000000000", "0230210C23B1A047BC9BDBB13448E67DEDDC108946DE6DE639BCC75D47C0216B1B", 1 },
	{  70,  69, "200000000000000000", "0290E6900A58D33393BC1097B5AED31F2E4E7CBD3E5466AF958665BC0121248483", 1 },
	{  75,  74, "4000000000000000000", "03726B574F193E374686D8E12BC6E4142ADEB06770E0A2856F5E4AD89F66044755", 1 },
	{  80,  79, "80000000000000000000", "037E1238F7B1CE757DF94FAA9A2EB261BF0AEB9F84DBF81212104E78931C2A19DC", 1 },
	{  85,  84, "1000000000000000000000", "0329C4574A4FD8C810B7E42A4B398882B381BCD85E40C6883712912D167C83E73A", 1 },
	{  90,  89, "20000000000000000000000", "035C38BD9AE4B10E8A250857006F3CFD98AB15A6196D9F4DFD25BC7ECC77D788D5", 1 },
	{  95,  94, "400000000000000000000000", "02967A5905D6F3B420959A02789F96AB4C3223A2C4D2762F817B7895C5BC88A045", 1 },
	{ 100,  99, "8000000000000000000000000", "03D2063D40402F030D4CC71331468827AA41A8A09BD6FD801BA77FB64F8E67E617", 1 },
	{ 105, 104, "100000000000000000000000000", "03BCF7CE887FFCA5E62C9CABBDB7FFA71DC183C52C04FF4EE5EE82E0C55C39D77B", 1 },
	{ 110, 109, "2000000000000000000000000000", "0309976BA5570966BF889196B7FDF5A0F9A1E9AB340556EC29F8BB60599616167D", 1 },
	{ 115, 114, "40000000000000000000000000000", "0248D313B0398D4923CDCA73B8CFA6532B91B96703902FC8B32FD438A3B7CD7F55", 1 },
	{ 120, 119, "800000000000000000000000000000", "02CEB6CBBCDBDF5EF7150682150F4CE2C6F4807B349827DCDBDD1F2EFA885A2630", 1 },
	{ 125, 124, "10000000000000000000000000000000", "0233709EB11E0D4439A729F21C2C443DEDB727528229713F0065721BA8FA46F00E", 1 },
	{ 130, 129, "200000000000000000000000000000000", "03633CBE3EC02B9401C5EFFA144C5B4D22F87940259634858FC7E59B1C09937852", 1 },
	{ 135, 134, "4000000000000000000000000000000000", "02145D2611C823A396EF6712CE0F712F09B9B4F3135E3E0AA3230FB9B6D08D1E16", 1 },
	//--- ALVOS REAIS (nao resolvidos: pubkey exposta on-chain em 2019, extraida
	//    e verificada direto da blockchain - a pubkey deriva de volta o endereco) ---
	{ 140, 139, "80000000000000000000000000000000000", "031F6A332D3C5C4F2DE2378C012F429CD109BA07D69690C6C701B6BB87860D6640", 0 },
	{ 145, 144, "1000000000000000000000000000000000000", "03AFDDA497369E219A2C1C369954A930E4D3740968E5E4352475BCFFCE3140DAE5", 0 },
	{ 150, 149, "20000000000000000000000000000000000000", "03137807790EA7DC6E97901C2BC87411F45ED74A5629315C4E4B03A0A102250C49", 0 },
	{ 155, 154, "400000000000000000000000000000000000000", "035CD1854CAE45391CA4EC428CC7E6C7D9984424B954209A8EEA197B9E364C05F6", 0 },
	{ 160, 159, "8000000000000000000000000000000000000000", "02E0A8B039282FAF6FE0FD769CFBC4B6B4CF8758BA68220EAC420E32B91DDFA673", 0 },
};
#define PUZZLE_CNT ((int)(sizeof(gPuzzleTable) / sizeof(gPuzzleTable[0])))

void ListPuzzles()
{
	printf("Puzzles disponiveis (use -puzzle N):\r\n");
	for (int i = 0; i < PUZZLE_CNT; i++)
		printf("  #%-4d range %-3d  %s\r\n", gPuzzleTable[i].num, gPuzzleTable[i].range,
			gPuzzleTable[i].solved ? "[RESOLVIDO - teste de validacao]" : "[EM ABERTO - alvo real, use -rnd]");
}

bool LoadPuzzle(int n)
{
	for (int i = 0; i < PUZZLE_CNT; i++)
	{
		if (gPuzzleTable[i].num == n)
		{
			gRange = gPuzzleTable[i].range;
			gStart.SetHexStr(gPuzzleTable[i].start);
			gStartSet = true;
			gPubKey.SetHexStr(gPuzzleTable[i].pubkey);
			if (!gDP)
				gDP = 16;
			gPuzzleNum = n;                          //usado no bloco de configuracao
			gPuzzleSolved = gPuzzleTable[i].solved;
			if (gRange > 80) //ranges grandes nao cabem na RAM para solve direto -> ativa loteria
				gRndMode = true;
			return true;
		}
	}
	return false;
}

#pragma pack(push, 1)
struct DBRec
{
	u8 x[12];
	u8 d[22];
	u8 type; //0 - tame, 1 - wild1, 2 - wild2
};
#pragma pack(pop)

// ===== Telemetria da GPU via NVML (temp/clock/fan) - RCKangaroo BR =====
// Le direto da placa ativa (qualquer modelo); limites de cor adaptados a cada GPU.
#ifdef HAVE_NVML
static nvmlDevice_t g_nvml_dev;
static bool g_nvml_ok = false;
static unsigned int g_throttle_temp = 87; // ponto de throttle lido da GPU (default 87)
static void nvml_tel_init()
{
	if (nvmlInit() != NVML_SUCCESS) return;
	unsigned int idx = (GpuCnt > 0) ? (unsigned int)GpuKangs[0]->CudaIndex : 0;
	if (nvmlDeviceGetHandleByIndex(idx, &g_nvml_dev) != NVML_SUCCESS) { nvmlShutdown(); return; }
	g_nvml_ok = true;
	unsigned int thr = 0;
	// ponto onde a GPU comeca a perder clock, adaptado a cada placa
	if (nvmlDeviceGetTemperatureThreshold(g_nvml_dev, NVML_TEMPERATURE_THRESHOLD_ACOUSTIC_CURR, &thr) == NVML_SUCCESS && thr >= 50 && thr <= 110)
		g_throttle_temp = thr;
	else if (nvmlDeviceGetTemperatureThreshold(g_nvml_dev, NVML_TEMPERATURE_THRESHOLD_SLOWDOWN, &thr) == NVML_SUCCESS && thr > 15)
		g_throttle_temp = thr - 15;
}
static bool nvml_tel_read(unsigned int* temp, unsigned int* clk, int* fan)
{
	if (!g_nvml_ok) return false;
	if (nvmlDeviceGetTemperature(g_nvml_dev, NVML_TEMPERATURE_GPU, temp) != NVML_SUCCESS) return false;
	if (nvmlDeviceGetClockInfo(g_nvml_dev, NVML_CLOCK_SM, clk) != NVML_SUCCESS) *clk = 0;
	unsigned int f = 0;
	*fan = (nvmlDeviceGetFanSpeed(g_nvml_dev, &f) == NVML_SUCCESS) ? (int)f : -1; // -1 = N/A (ex: laptop)
	return true;
}
static void nvml_tel_shutdown() { if (g_nvml_ok) nvmlShutdown(); }
#else
static void nvml_tel_init() {}
static bool nvml_tel_read(unsigned int* t, unsigned int* c, int* f) { (void)t; (void)c; (void)f; return false; }
static void nvml_tel_shutdown() {}
#endif

void InitGpus()
{
	GpuCnt = 0;
	int gcnt = 0;
	cudaGetDeviceCount(&gcnt);
	if (gcnt > MAX_GPU_CNT)
		gcnt = MAX_GPU_CNT;

//	gcnt = 1; //dbg
	if (!gcnt)
		return;

	int drv, rt;
	cudaRuntimeGetVersion(&rt);
	cudaDriverGetVersion(&drv);
	char drvver[100];
	sprintf(drvver, "%d.%d/%d.%d", drv / 1000, (drv % 100) / 10, rt / 1000, (rt % 100) / 10);

	(void)drvver; // info de CUDA/GPU exibida no bloco de configuracao (print_config)
	cudaError_t cudaStatus;
	for (int i = 0; i < gcnt; i++)
	{
		cudaStatus = cudaSetDevice(i);
		if (cudaStatus != cudaSuccess)
		{
			printf("cudaSetDevice falhou para a GPU %d!\r\n", i);
			continue;
		}

		if (!gGPUs_Mask[i])
			continue;

		cudaDeviceProp deviceProp;
		cudaGetDeviceProperties(&deviceProp, i);
		if (deviceProp.major < 6)
		{
			printf("GPU %d - nao suportada, ignorando\r\n", i);
			continue;
		}

		cudaSetDeviceFlags(cudaDeviceScheduleBlockingSync);

		GpuKangs[GpuCnt] = new RCGpuKang();
		GpuKangs[GpuCnt]->CudaIndex = i;
		GpuKangs[GpuCnt]->persistingL2CacheMaxSize = deviceProp.persistingL2CacheMaxSize;
		GpuKangs[GpuCnt]->mpCnt = deviceProp.multiProcessorCount;
		GpuKangs[GpuCnt]->IsOldGpu = deviceProp.l2CacheSize < 16 * 1024 * 1024;
		GpuCnt++;
	}
	// total de GPUs mostrado no bloco de configuracao (print_config)
}
#ifdef _WIN32
u32 __stdcall kang_thr_proc(void* data)
{
	RCGpuKang* Kang = (RCGpuKang*)data;
	Kang->Execute();
	InterlockedDecrement(&ThrCnt);
	return 0;
}
#else
void* kang_thr_proc(void* data)
{
	RCGpuKang* Kang = (RCGpuKang*)data;
	Kang->Execute();
	__sync_fetch_and_sub(&ThrCnt, 1);
	return 0;
}
#endif
void AddPointsToList(u32* data, int pnt_cnt, u64 ops_cnt)
{
	csAddPoints.Enter();
	if (PntIndex + pnt_cnt >= MAX_CNT_LIST)
	{
		csAddPoints.Leave();
		printf("Overflow do buffer de DPs, pontos perdidos, aumente o valor de DP!\r\n");
		return;
	}
	memcpy(pPntList + GPU_DP_SIZE * PntIndex, data, pnt_cnt * GPU_DP_SIZE);
	PntIndex += pnt_cnt;
	PntTotalOps += ops_cnt;
	csAddPoints.Leave();
}

bool Collision_SOTA(EcPoint& pnt, EcInt t, int TameType, EcInt w, int WildType, bool IsNeg)
{
	if (IsNeg)
		t.Neg();
	if (TameType == TAME)
	{
		gPrivKey = t;
		gPrivKey.Sub(w);
		EcInt sv = gPrivKey;
		gPrivKey.Add(Int_HalfRange);
		EcPoint P = ec.MultiplyG(gPrivKey);
		if (P.IsEqual(pnt))
			return true;
		gPrivKey = sv;
		gPrivKey.Neg();
		gPrivKey.Add(Int_HalfRange);
		P = ec.MultiplyG(gPrivKey);
		return P.IsEqual(pnt);
	}
	else
	{
		gPrivKey = t;
		gPrivKey.Sub(w);
		if (gPrivKey.data[4] >> 63)
			gPrivKey.Neg();
		gPrivKey.ShiftRight(1);
		EcInt sv = gPrivKey;
		gPrivKey.Add(Int_HalfRange);
		EcPoint P = ec.MultiplyG(gPrivKey);
		if (P.IsEqual(pnt))
			return true;
		gPrivKey = sv;
		gPrivKey.Neg();
		gPrivKey.Add(Int_HalfRange);
		P = ec.MultiplyG(gPrivKey);
		return P.IsEqual(pnt);
	}
}


void CheckNewPoints()
{
	csAddPoints.Enter();
	if (!PntIndex)
	{
		csAddPoints.Leave();
		return;
	}

	int cnt = PntIndex;
	memcpy(pPntList2, pPntList, GPU_DP_SIZE * cnt);
	PntIndex = 0;
	csAddPoints.Leave();

	for (int i = 0; i < cnt; i++)
	{
		DBRec nrec;
		u8* p = pPntList2 + i * GPU_DP_SIZE;
		memcpy(nrec.x, p, 12);
		memcpy(nrec.d, p + 16, 22);
		nrec.type = gGenMode ? TAME : p[40];

		DBRec* pref = (DBRec*)db.FindOrAddDataBlock((u8*)&nrec);
		if (gGenMode)
			continue;
		if (pref)
		{
			//in db we dont store first 3 bytes so restore them
			DBRec tmp_pref;
			memcpy(&tmp_pref, &nrec, 3);
			memcpy(((u8*)&tmp_pref) + 3, pref, sizeof(DBRec) - 3);
			pref = &tmp_pref;

			if (pref->type == nrec.type)
			{
				if (pref->type == TAME)
					continue;

				//if it's wild, we can find the key from the same type if distances are different
				if (*(u64*)pref->d == *(u64*)nrec.d)
					continue;
				//else
				//	ToLog("key found by same wild");
			}

			EcInt w, t;
			int TameType, WildType;
			if (pref->type != TAME)
			{
				memcpy(w.data, pref->d, sizeof(pref->d));
				if (pref->d[21] == 0xFF) memset(((u8*)w.data) + 22, 0xFF, 18);
				memcpy(t.data, nrec.d, sizeof(nrec.d));
				if (nrec.d[21] == 0xFF) memset(((u8*)t.data) + 22, 0xFF, 18);
				TameType = nrec.type;
				WildType = pref->type;
			}
			else
			{
				memcpy(w.data, nrec.d, sizeof(nrec.d));
				if (nrec.d[21] == 0xFF) memset(((u8*)w.data) + 22, 0xFF, 18);
				memcpy(t.data, pref->d, sizeof(pref->d));
				if (pref->d[21] == 0xFF) memset(((u8*)t.data) + 22, 0xFF, 18);
				TameType = TAME;
				WildType = nrec.type;
			}

			bool res = Collision_SOTA(gPntToSolve, t, TameType, w, WildType, false) || Collision_SOTA(gPntToSolve, t, TameType, w, WildType, true);
			if (!res)
			{
				bool w12 = ((pref->type == WILD1) && (nrec.type == WILD2)) || ((pref->type == WILD2) && (nrec.type == WILD1));
				if (w12) //in rare cases WILD and WILD2 can collide in mirror, in this case there is no way to find K
					;// ToLog("W1 and W2 collides in mirror");
				else
				{
					printf("Erro de colisao\r\n");
					gTotalErrors++;
				}
				continue;
			}
			gSolved = true;
			break;
		}
	}
}

void ShowStats(u64 tm_start, double exp_ops, double dp_val)
{
#ifdef DEBUG_MODE
	for (int i = 0; i <= MD_LEN; i++)
	{
		u64 val = 0;
		for (int j = 0; j < GpuCnt; j++)
		{
			val += GpuKangs[j]->dbg[i];
		}
		if (val)
			printf("Loop size %d: %llu\r\n", i, val);
	}
#endif

	int speed = GpuKangs[0]->GetStatsSpeed();
	for (int i = 1; i < GpuCnt; i++)
		speed += GpuKangs[i]->GetStatsSpeed();

	u64 est_dps_cnt = (u64)(exp_ops / dp_val);
	u64 exp_sec = 0xFFFFFFFFFFFFFFFFull;
	if (speed)
		exp_sec = (u64)((exp_ops / 1000000) / speed); //in sec
	u64 exp_days = exp_sec / (3600 * 24);
	int exp_hours = (int)(exp_sec - exp_days * (3600 * 24)) / 3600;
	int exp_min = (int)(exp_sec - exp_days * (3600 * 24) - exp_hours * 3600) / 60;
	int exp_secs = (int)(exp_sec % 60);

	u64 sec = (GetTickCount64() - tm_start) / 1000;
	u64 days = sec / (3600 * 24);
	int hours = (int)(sec - days * (3600 * 24)) / 3600;
	int min = (int)(sec - days * (3600 * 24) - hours * 3600) / 60;
	int secs = (int)(sec % 60);

	const char* pfx = gGenMode ? "GERA " : (IsBench ? "BENCH" : "PRINC");
	const char* errcol = gTotalErrors ? "\033[1;31m" : "\033[2m"; // vermelho se houver erro
	char gpubuf[192]; gpubuf[0] = 0;
#ifdef HAVE_NVML
	{
		unsigned int gt = 0, gc = 0; int gf = -1;
		if (nvml_tel_read(&gt, &gc, &gf)) {
			const char* tcol = (gt >= g_throttle_temp) ? "\033[1;31m" : ((gt + 7 >= g_throttle_temp) ? "\033[1;33m" : "\033[1;32m");
			const char* ccol = (gt >= g_throttle_temp) ? "\033[31m" : "\033[34m";
			char fanbuf[16];
			if (gf >= 0) snprintf(fanbuf, sizeof(fanbuf), "%d%%", gf); else snprintf(fanbuf, sizeof(fanbuf), "N/A");
			snprintf(gpubuf, sizeof(gpubuf), "\033[2m|\033[0m  GPU %s%u\xc2\xb0""C\033[0m \033[2m|\033[0m %s%u MHz\033[0m \033[2m|\033[0m Fan \033[2m%s\033[0m  ", tcol, gt, ccol, gc, fanbuf);
		}
	}
#endif
	printf("\r\033[2m[\033[0m\033[1;35m%s\033[0m\033[2m]\033[0m \033[1;36m%d\033[0m \033[2mMKeys/s\033[0m  \033[2m|\033[0m  Saltos: \033[1m%s\033[0m  \033[2m|\033[0m  DPs: \033[1;37m%lluK\033[0m\033[2m/%lluK\033[0m  \033[2m|\033[0m  Err: %s%d\033[0m  %s\033[2m|\033[0m  \033[33m%llud %02d:%02d:%02d\033[0m\033[2m/ETA %llud %02d:%02d:%02d\033[0m\033[K",
		pfx, speed, ui_num(PntTotalOps), db.GetBlockCnt() / 1000, est_dps_cnt / 1000, errcol, gTotalErrors, gpubuf, days, hours, min, secs, exp_days, exp_hours, exp_min, exp_secs);
	fflush(stdout);
}

void DrawSolvePanel(u64 tm_start, double exp_ops, double dp_val, bool first); //fwd decl (definido apos os helpers de UI)

bool SolvePoint(EcPoint PntToSolve, int Range, int DP, EcInt* pk_res)
{
	if ((Range < 32) || (Range > 180))
	{
		printf("Valor de Range nao suportado (%d)!\r\n", Range);
		return false;
	}
	if ((DP < 14) || (DP > 60)) 
	{
		printf("Valor de DP nao suportado (%d)!\r\n", DP);
		return false;
	}

	if (!gQuiet) printf("\r\nResolvendo ponto: Range %d bits, DP %d, iniciando...\r\n", Range, DP);
	double ops = 1.15 * pow(2.0, Range / 2.0);
	double dp_val = (double)(1ull << DP);
	double ram = (32 + 4 + 4) * ops / dp_val; //+4 for grow allocation and memory fragmentation
	ram += sizeof(TListRec) * 256 * 256 * 256; //3byte-prefix table
	ram /= (1024 * 1024 * 1024); //GB
	if (!gQuiet) printf("Metodo SOTA, ops estimadas: 2^%.3f, RAM para DPs: %.3f GB. Sem incluir overheads de DP/GPU!\r\n", log2(ops), ram);
	gIsOpsLimit = false;
	double MaxTotalOps = 0.0;
	if (gMax > 0)
	{
		MaxTotalOps = gMax * ops;
		double ram_max = (32 + 4 + 4) * MaxTotalOps / dp_val; //+4 for grow allocation and memory fragmentation
		ram_max += sizeof(TListRec) * 256 * 256 * 256; //3byte-prefix table
		ram_max /= (1024 * 1024 * 1024); //GB
		if (!gQuiet) printf("Maximo de ops permitido: 2^%.3f, RAM max para DPs: %.3f GB\r\n", log2(MaxTotalOps), ram_max);
	}

	u64 total_kangs = GpuKangs[0]->CalcKangCnt();
	for (int i = 1; i < GpuCnt; i++)
		total_kangs += GpuKangs[i]->CalcKangCnt();
	double path_single_kang = ops / total_kangs;	
	double DPs_per_kang = path_single_kang / dp_val;
	if (!gQuiet) printf("DPs estimados por canguru: %.3f.%s\r\n", DPs_per_kang, (DPs_per_kang < 5) ? " Overhead de DP alto, use um valor de DP menor se possivel!" : "");

	if (!gGenMode && gTamesFileName[0])
	{
		printf("carregando tames...\r\n");
		if (db.LoadFromFile(gTamesFileName))
		{
			printf("tames carregados\r\n");
			if (db.Header[0] != gRange)
			{
				printf("tames carregados tem range diferente, nao podem ser usados, limpando\r\n");
				db.Clear();
			}
		}
		else
			printf("falha ao carregar tames\r\n");
	}

	SetRndSeed(0); //use same seed to make tames from file compatible
	PntTotalOps = 0;
	PntIndex = 0;
//prepare jumps
	EcInt minjump, t;
	minjump.Set(1);
	minjump.ShiftLeft(Range / 2 + 3);
	for (int i = 0; i < JMP_CNT; i++)
	{
		EcJumps1[i].dist = minjump;
		t.RndMax(minjump);
		EcJumps1[i].dist.Add(t);
		EcJumps1[i].dist.data[0] &= 0xFFFFFFFFFFFFFFFE; //must be even
		EcJumps1[i].p = ec.MultiplyG(EcJumps1[i].dist);
	}

	minjump.Set(1);
	minjump.ShiftLeft(Range - 10); //large jumps for L1S2 loops. Must be almost RANGE_BITS
	for (int i = 0; i < JMP_CNT; i++)
	{
		EcJumps2[i].dist = minjump;
		t.RndMax(minjump);
		EcJumps2[i].dist.Add(t);
		EcJumps2[i].dist.data[0] &= 0xFFFFFFFFFFFFFFFE; //must be even
		EcJumps2[i].p = ec.MultiplyG(EcJumps2[i].dist);
	}

	minjump.Set(1);
	minjump.ShiftLeft(Range - 10 - 2); //large jumps for loops >2
	for (int i = 0; i < JMP_CNT; i++)
	{
		EcJumps3[i].dist = minjump;
		t.RndMax(minjump);
		EcJumps3[i].dist.Add(t);
		EcJumps3[i].dist.data[0] &= 0xFFFFFFFFFFFFFFFE; //must be even
		EcJumps3[i].p = ec.MultiplyG(EcJumps3[i].dist);
	}
	SetRndSeed(GetTickCount64());

	Int_HalfRange.Set(1);
	Int_HalfRange.ShiftLeft(Range - 1);
	Pnt_HalfRange = ec.MultiplyG(Int_HalfRange);
	Pnt_NegHalfRange = Pnt_HalfRange;
	Pnt_NegHalfRange.y.NegModP();
	Int_TameOffset.Set(1);
	Int_TameOffset.ShiftLeft(Range - 1);
	EcInt tt;
	tt.Set(1);
	tt.ShiftLeft(Range - 5); //half of tame range width
	Int_TameOffset.Sub(tt);
	gPntToSolve = PntToSolve;

//prepare GPUs
	for (int i = 0; i < GpuCnt; i++)
		if (!GpuKangs[i]->Prepare(PntToSolve, Range, DP, EcJumps1, EcJumps2, EcJumps3))
		{
			GpuKangs[i]->Failed = true;
			printf("GPU %d: preparacao falhou\r\n", GpuKangs[i]->CudaIndex);
		}

	u64 tm0 = GetTickCount64();
	if (!gQuiet) printf("GPUs iniciadas...\r\n");

#ifdef _WIN32
	HANDLE thr_handles[MAX_GPU_CNT];
#else
	pthread_t thr_handles[MAX_GPU_CNT];
#endif

	u32 ThreadID;
	gSolved = false;
	ThrCnt = GpuCnt;
	for (int i = 0; i < GpuCnt; i++)
	{
#ifdef _WIN32
		thr_handles[i] = (HANDLE)_beginthreadex(NULL, 0, kang_thr_proc, (void*)GpuKangs[i], 0, &ThreadID);
#else
		pthread_create(&thr_handles[i], NULL, kang_thr_proc, (void*)GpuKangs[i]);
#endif
	}

	u64 tm_stats = GetTickCount64();
	bool panel_first = true; //1o desenho do painel nao sobe linhas - RCKangaroo BR
	while (!gSolved)
	{
		CheckNewPoints();
		Sleep(10);
		if (GetTickCount64() - tm_stats > 10 * 1000)
		{
			if (gPanelMode) { DrawSolvePanel(tm0, ops, dp_val, panel_first); panel_first = false; }
			else ShowStats(tm0, ops, dp_val);
			tm_stats = GetTickCount64();
		}

		if ((MaxTotalOps > 0.0) && (PntTotalOps > MaxTotalOps))
		{
			gIsOpsLimit = true;
			if (!gQuiet) printf("\r\nLimite de operacoes atingido\r\n");
			break;
		}
	}

	if (!gQuiet) printf("\r\nEncerrando o trabalho ...\r\n");
	for (int i = 0; i < GpuCnt; i++)
		GpuKangs[i]->Stop();
	while (ThrCnt)
		Sleep(10);
	for (int i = 0; i < GpuCnt; i++)
	{
#ifdef _WIN32
		CloseHandle(thr_handles[i]);
#else
		pthread_join(thr_handles[i], NULL);
#endif
	}

	if (gIsOpsLimit)
	{
		if (gGenMode)
		{
			printf("salvando tames...\r\n");
			db.Header[0] = gRange; 
			if (db.SaveToFile(gTamesFileName))
				printf("tames salvos\r\n");
			else
				printf("falha ao salvar tames\r\n");
		}
		db.Clear();
		return false;
	}

	double K = (double)PntTotalOps / pow(2.0, Range / 2.0);
	u64 solsec = (GetTickCount64() - tm0) / 1000;
	double avg_jump = 1.5 * pow(2.0, Range / 2.0 + 3.0); // salto medio de cada operacao (~1.5x minjump)
	double tot_dist = (double)PntTotalOps * avg_jump;    // distancia total percorrida pelos cangurus
	if (!gQuiet) {
		printf("Ponto resolvido, K: %.3f (com overheads de DP e GPU)\r\n", K);
		printf("  \033[2m->\033[0m \033[1m%s\033[0m chaves testadas em \033[1m%llud %02d:%02d:%02d\033[0m\r\n",
			ui_num(PntTotalOps), solsec / 86400, (int)((solsec % 86400) / 3600), (int)((solsec % 3600) / 60), (int)(solsec % 60));
		printf("  \033[2m->\033[0m cada operacao PULA ~\033[1;36m2^%.0f\033[0m chaves  \033[2m|\033[0m  distancia total percorrida: ~\033[1;36m2^%.1f\033[0m chaves \033[2m(forca bruta testaria 1 chave por operacao)\033[0m\r\n\r\n",
			log2(avg_jump), log2(tot_dist));
	}
	db.Clear();
	*pk_res = gPrivKey;
	return true;
}

bool ParseCommandLine(int argc, char* argv[])
{
	int ci = 1;
	while (ci < argc)
	{
		char* argument = argv[ci];
		ci++;
		if (strcmp(argument, "-gpu") == 0)
		{
			if (ci >= argc)
			{
				printf("erro: faltou o valor apos a opcao -gpu\r\n");
				return false;
			}
			char* gpus = argv[ci];
			ci++;
			memset(gGPUs_Mask, 0, sizeof(gGPUs_Mask));
			for (int i = 0; i < (int)strlen(gpus); i++)
			{
				if ((gpus[i] < '0') || (gpus[i] > '9'))
				{
					printf("erro: valor invalido para a opcao -gpu\r\n");
					return false;
				}
				gGPUs_Mask[gpus[i] - '0'] = 1;
			}
		}
		else
		if (strcmp(argument, "-dp") == 0)
		{
			int val = atoi(argv[ci]);
			ci++;
			if ((val < 14) || (val > 60))
			{
				printf("erro: valor invalido para a opcao -dp\r\n");
				return false;
			}
			gDP = val;
		}
		else
		if (strcmp(argument, "-range") == 0)
		{
			int val = atoi(argv[ci]);
			ci++;
			if ((val < 32) || (val > 170))
			{
				printf("erro: valor invalido para a opcao -range\r\n");
				return false;
			}
			gRange = val;
		}
		else
		if (strcmp(argument, "-start") == 0)
		{	
			if (!gStart.SetHexStr(argv[ci]))
			{
				printf("erro: valor invalido para a opcao -start\r\n");
				return false;
			}
			ci++;
			gStartSet = true;
		}
		else
		if (strcmp(argument, "-pubkey") == 0)
		{
			if (!gPubKey.SetHexStr(argv[ci]))
			{
				printf("erro: valor invalido para a opcao -pubkey\r\n");
				return false;
			}
			ci++;
		}
		else
		if (strcmp(argument, "-tames") == 0)
		{
			strcpy(gTamesFileName, argv[ci]);
			ci++;
		}
		else
		if (strcmp(argument, "-max") == 0)
		{
			double val = atof(argv[ci]);
			ci++;
			if (val < 0.001)
			{
				printf("erro: valor invalido para a opcao -max\r\n");
				return false;
			}
			gMax = val;
		}
		else
		if (strcmp(argument, "-puzzle") == 0)
		{
			int val = atoi(argv[ci]);
			ci++;
			if (!LoadPuzzle(val))
			{
				printf("erro: puzzle #%d nao esta na tabela do RCKangaroo BR.\r\n", val);
				ListPuzzles();
				return false;
			}
		}
		else
		if (strcmp(argument, "-rnd") == 0)
		{
			gRndMode = true;
		}
		else
		if (strcmp(argument, "-no-clear") == 0 || strcmp(argument, "--no-clear") == 0)
		{
			// tratado no pre-scan do main(); aqui so evita o erro "opcao desconhecida"
		}
		else
		if (strcmp(argument, "-wbits") == 0)
		{
			int val = atoi(argv[ci]);
			ci++;
			if ((val < 32) || (val > 169))
			{
				printf("erro: valor invalido para a opcao -wbits (use 32..169)\r\n");
				return false;
			}
			gWBits = val;
		}
		else
		{
			printf("erro: opcao desconhecida %s\r\n", argument);
			return false;
		}
	}
	if (!gPubKey.x.IsZero())
		if (!gStartSet || !gRange || !gDP)
		{
			printf("erro: voce tambem precisa especificar -dp, -range e -start\r\n");
			return false;
		}
	if (gRndMode)
	{
		if (gPubKey.x.IsZero())
		{
			printf("erro: o modo -rnd exige -pubkey (o alvo), alem de -start e -range\r\n");
			return false;
		}
		if (!gWBits)
			gWBits = 72; //janela padrao - o motor Kangaroo so e eficiente a partir de ~70 bits
		if (gWBits < 70) //TRAVA: abaixo disso o motor nao resolve (overhead de DP com ~1M cangurus)
		{
			printf("erro: -wbits (%d) muito pequeno. O motor Kangaroo so resolve janelas >= 70 bits.\r\n", gWBits);
			printf("      Com ~1M cangurus, janelas menores tem overhead de DP enorme e nao acham a chave.\r\n");
			printf("      Use -wbits >= 72.\r\n");
			return false;
		}
		if (gWBits >= (int)gRange)
		{
			printf("erro: -wbits (%d) deve ser menor que -range (%d)\r\n", gWBits, gRange);
			return false;
		}
		if (gMax == 0.0)
			gMax = 3.0; //limite de ops por janela sorteada (menor = abandona janelas erradas mais rapido)
	}
	if (gTamesFileName[0] && !IsFileExist(gTamesFileName))
	{
		if (gMax == 0.0)
		{
			printf("erro: voce tambem precisa da opcao -max para gerar tames\r\n");
			return false;
		}
		gGenMode = true;
	}
	return true;
}

// ===== Helpers de interface (largura adaptavel ao terminal) - RCKangaroo BR =====
#ifndef _WIN32
#include <sys/ioctl.h>
#include <unistd.h>
#endif
int gUIW = 120; // largura da interface; ajustada ao terminal em tempo de execucao
static void ui_detect_width()
{
#ifndef _WIN32
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
		gUIW = ws.ws_col;
#endif
	if (gUIW < 80) gUIW = 80;    // minimo utilizavel
	if (gUIW > 220) gUIW = 220;  // teto de seguranca
}
static void ui_rep(const char* s, int n) { for (int i = 0; i < n; i++) printf("%s", s); }
// formata numero grande com separador de milhar (pt-BR usa ponto)
static const char* ui_num(unsigned long long v)
{
	static char out[48]; char tmp[32];
	snprintf(tmp, sizeof(tmp), "%llu", v);
	int n = (int)strlen(tmp), oi = 0;
	for (int i = 0; i < n; i++) {
		out[oi++] = tmp[i];
		int rem = n - 1 - i;
		if (rem > 0 && rem % 3 == 0) out[oi++] = '.';
	}
	out[oi] = 0;
	return out;
}
// formata uma porcentagem como decimal real (com todos os zeros), sem notacao cientifica
static const char* ui_pct(double v)
{
	static char buf[80];
	if (v <= 0) { snprintf(buf, sizeof(buf), "0"); return buf; }
	if (v >= 0.01) { snprintf(buf, sizeof(buf), "%.4g", v); return buf; } // ex: 5.46 , 0.781
	int lead = (int)floor(log10(v));   // ex: -20 para 1.41e-20
	int prec = -lead + 2;              // ~3 algarismos significativos
	if (prec > 60) prec = 60;
	if (prec < 2) prec = 2;
	snprintf(buf, sizeof(buf), "%.*f", prec, v); // ex: 0.0000000000000000000141
	return buf;
}
// formata um inteiro grande (double, p/ valores > u64 como 2^67) com separador de milhar
static const char* ui_num_d(double v)
{
	static char out[80]; char tmp[48];
	snprintf(tmp, sizeof(tmp), "%.0f", v); // potencia de 2 e exata em double -> digitos exatos
	int n = (int)strlen(tmp), oi = 0;
	for (int i = 0; i < n; i++) {
		out[oi++] = tmp[i];
		int rem = n - 1 - i;
		if (rem > 0 && rem % 3 == 0) out[oi++] = '.';
	}
	out[oi] = 0;
	return out;
}
// formata um numero muito grande (double) com 6 algarismos significativos + zeros + separador de milhar
static const char* ui_bignum(double v)
{
	static char out[96];
	if (v < 1.0) { snprintf(out, sizeof(out), "0"); return out; }
	int L = (int)floor(log10(v)); int sig = 6;
	double scaled = v / pow(10.0, (double)(L - (sig - 1))); // ~100000..999999
	long m = (long)(scaled + 0.5);
	char digits[24]; snprintf(digits, sizeof(digits), "%ld", m);
	int nd = (int)strlen(digits); int total = L + 1; if (nd > total) total = nd;
	int zeros = total - nd; if (zeros < 0) zeros = 0;
	char full[80]; int fi = 0;
	for (int i = 0; i < nd && fi < 72; i++) full[fi++] = digits[i];
	for (int i = 0; i < zeros && fi < 72; i++) full[fi++] = '0';
	int n = fi, oi = 0;
	for (int i = 0; i < n; i++) { out[oi++] = full[i]; int rem = n - 1 - i; if (rem > 0 && rem % 3 == 0) out[oi++] = '.'; }
	out[oi] = 0;
	return out;
}
// palavra de escala pt-BR para 10^(3k)
static const char* scale_word(int k)
{
	switch (k) {
	case 2: return "milhoes"; case 3: return "bilhoes"; case 4: return "trilhoes";
	case 5: return "quatrilhoes"; case 6: return "quintilhoes"; case 7: return "sextilhoes";
	case 8: return "septilhoes"; case 9: return "octilhoes"; case 10: return "nonilhoes";
	default: return "";
	}
}
// linha do banner com texto centralizado entre asteriscos
static void ui_banner(const char* text, const char* color)
{
	int tl = (int)strlen(text); int pad = gUIW - 2 - tl; if (pad < 0) pad = 0;
	int l = pad / 2, r = pad - l;
	printf("*%s", color);
	for (int i = 0; i < l; i++) putchar(' ');
	printf("%s", text);
	for (int i = 0; i < r; i++) putchar(' ');
	printf("\033[0m*\r\n");
}
// regua horizontal (com titulo centralizado se fornecido)
static void ui_rule(const char* title)
{
	if (!title) { ui_rep("\xe2\x95\x90", gUIW); printf("\r\n"); return; } // U+2550 ═
	int tl = (int)strlen(title); int dashes = gUIW - tl - 2; if (dashes < 0) dashes = 0;
	int l = dashes / 2, r = dashes - l;
	ui_rep("\xe2\x95\x90", l);
	printf(" \033[1;33m%s\033[0m ", title);
	ui_rep("\xe2\x95\x90", r);
	printf("\r\n");
}

// Painel fixo do solve direto - mesmo visual do modo loteria (regua + barra + regua) - RCKangaroo BR
// A barra representa o progresso em relacao ao esperado (ops feitas / ops esperadas p/ K=1.0).
void DrawSolvePanel(u64 tm_start, double exp_ops, double dp_val, bool first)
{
	double prog = (exp_ops > 0.0) ? (double)PntTotalOps / exp_ops : 0.0;
	double pbar = prog; if (pbar < 0.0) pbar = 0.0; if (pbar > 1.0) pbar = 1.0; //barra trava em 100%
	int bw = gUIW - 14; if (bw < 40) bw = 40; if (bw > 210) bw = 210; //nº de celulas da barra
	int filled = (int)(pbar * bw + 0.5); if (filled > bw) filled = bw;
	// monta a barra: verde=feito (bloco solido U+2588), apagado=restante ('.')
	char bar[2048]; int bi = 0;
	memcpy(bar + bi, "\033[1;32m", 7); bi += 7;
	for (int b = 0; b < bw; b++) {
		if (b == filled) { memcpy(bar + bi, "\033[2m", 4); bi += 4; } //troca p/ apagado
		if (b < filled) { bar[bi++] = '\xe2'; bar[bi++] = '\x96'; bar[bi++] = '\x88'; }
		else bar[bi++] = '.';
	}
	memcpy(bar + bi, "\033[0m", 4); bi += 4; bar[bi] = 0;
	char title[96];
	if (gPuzzleNum > 0) snprintf(title, sizeof(title), "RESOLVENDO PUZZLE #%d - range %d bits", gPuzzleNum, gRange);
	else snprintf(title, sizeof(title), "RESOLVENDO - range %d bits", gRange);
	if (!first) printf("\033[4A"); //sobe 4 linhas p/ redesenhar o painel no mesmo lugar
	printf("\r");
	ui_rule(title);                                                                          //linha 1
	printf("  Progresso \033[1m~%.1f%%\033[0m do esperado   \033[2m\xc2\xb7\033[0m   ops 2^%.2f / 2^%.2f%s\033[K\r\n", //linha 2
		prog * 100.0, (PntTotalOps > 0 ? log2((double)PntTotalOps) : 0.0), log2(exp_ops),
		prog > 1.0 ? "   \033[33m(alem do esperado - a sorte ainda nao bateu)\033[0m" : "");
	printf("  Busca:   [%s]\033[K\r\n", bar);                                                 //linha 3 (cores embutidas)
	ui_rule(NULL);                                                                            //linha 4
	ShowStats(tm_start, exp_ops, dp_val);                                                     //linha 5 (status, com \r)
}

// Converte uma string hexadecimal (big-endian, ate 64 digitos) em decimal - RCKangaroo BR
static void hex_to_dec(const char* hex, char* out)
{
	unsigned char n[32]; memset(n, 0, sizeof(n));
	int len = (int)strlen(hex); int bi = 31;
	for (int i = len - 1; i >= 0 && bi >= 0; ) {
		int c = hex[i]; i--;
		int lo = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : 0;
		int hi = 0;
		if (i >= 0) { c = hex[i]; i--; hi = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : 0; }
		n[bi--] = (unsigned char)((hi << 4) | lo);
	}
	char tmp[128]; int ti = 0; bool allzero;
	do { // divisao longa por 10, dig a dig
		int rem = 0; allzero = true;
		for (int i = 0; i < 32; i++) { int cur = (rem << 8) | n[i]; n[i] = (unsigned char)(cur / 10); rem = cur % 10; if (n[i]) allzero = false; }
		tmp[ti++] = (char)('0' + rem);
	} while (!allzero && ti < 120);
	int oi = 0; for (int i = ti - 1; i >= 0; i--) out[oi++] = tmp[i]; out[oi] = 0;
}

// Salva o resultado em RESULTS.TXT como um bloco vertical completo (append) - RCKangaroo BR
static void SaveResult(const char* priv_hex, EcPoint& pub, int puzzle, int range,
	bool rnd, int wbits, u64 windows, u64 ops, double K, u64 secs, double dist)
{
	FILE* fp = fopen("RESULTS.TXT", "a");
	if (!fp) { printf("\033[1;31mAVISO: nao foi possivel salvar em RESULTS.TXT!\033[0m\r\n"); return; }
	time_t tt = time(NULL); struct tm* lt = localtime(&tt);
	char ts[32]; strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", lt);
	char xhex[80]; pub.x.GetHexStr(xhex);
	const char* pfx = (pub.y.data[0] & 1) ? "03" : "02"; // paridade de Y -> prefixo comprimido
	const char* shortk = priv_hex; while (*shortk == '0' && *(shortk + 1)) shortk++; // tira zeros a esquerda
	char dec[128]; hex_to_dec(priv_hex, dec);
	const char* LINE = "================================================================";
	const char* SEP  = "----------------------------------------------------------------";
	fprintf(fp, "%s\n  RCKangaroo BR  -  CHAVE ENCONTRADA\n  %s\n%s\n", LINE, ts, LINE);
	if (puzzle > 0) fprintf(fp, "  Puzzle ............. #%d  (range %d bits)\n", puzzle, range);
	else            fprintf(fp, "  Alvo ............... range %d bits\n", range);
	if (rnd) fprintf(fp, "  Modo ............... LOTERIA (janelas de %d bits)\n", wbits);
	else     fprintf(fp, "  Modo ............... SOLVE DIRETO\n");
	fprintf(fp, "\n  Chave publica (comprimida):\n  %s%s\n", pfx, xhex);
	fprintf(fp, "\n  CHAVE PRIVADA (hex, 64 digitos):\n  %s\n", priv_hex);
	fprintf(fp, "\n  CHAVE PRIVADA (hex curto):\n  %s\n", shortk);
	fprintf(fp, "\n  CHAVE PRIVADA (decimal):\n  %s\n", dec);
	fprintf(fp, "%s\n", SEP);
	fprintf(fp, "  Chaves avaliadas ... %s  (saltos)\n", ui_num(ops));
	if (rnd)   fprintf(fp, "  Janelas testadas ... %llu\n", windows);
	if (K >= 0.0) fprintf(fp, "  Fator K ............ %.2f\n", K);
	fprintf(fp, "  Tempo .............. %llud %02d:%02d:%02d\n", secs / 86400, (int)((secs % 86400) / 3600), (int)((secs % 3600) / 60), (int)(secs % 60));
	fprintf(fp, "  Distancia .......... ~%s chaves  (~2^%.0f)\n", ui_bignum(dist), log2(dist));
	fprintf(fp, "%s\n\n", LINE);
	fclose(fp);
	// confirma na tela ONDE o arquivo foi salvo (caminho absoluto) - RCKangaroo BR
	char abspath[1200];
#ifdef _WIN32
	if (!_fullpath(abspath, "RESULTS.TXT", sizeof(abspath))) snprintf(abspath, sizeof(abspath), "RESULTS.TXT");
#else
	char cwd[1024];
	if (getcwd(cwd, sizeof(cwd))) snprintf(abspath, sizeof(abspath), "%s/RESULTS.TXT", cwd);
	else snprintf(abspath, sizeof(abspath), "RESULTS.TXT");
#endif
	printf("\033[1;32m\xe2\x9c\x94 Resultado salvo em:\033[0m %s\r\n\r\n", abspath);
}

// Bloco de configuracao (sem moldura) - RCKangaroo BR
void print_config()
{
	const char* D = "\033[2m", *R = "\033[0m", *W = "\033[1;37m", *A = "\033[1;33m", *G = "\033[32m", *B = "\033[34m";
	int idx = (GpuCnt > 0) ? GpuKangs[0]->CudaIndex : 0;
	cudaDeviceProp dp; cudaGetDeviceProperties(&dp, idx);
	int drv = 0, rt = 0; cudaDriverGetVersion(&drv); cudaRuntimeGetVersion(&rt);
	printf("\r\n");
	printf("  %sGPU   %s %s%s%s %s\xc2\xb7%s %.2f GB %s\xc2\xb7%s %d CUs %s\xc2\xb7%s cap %d.%d %s\xc2\xb7%s L2 %d KB\r\n",
		D, R, W, dp.name, R, D, R, (double)dp.totalGlobalMem / (1024.0 * 1024.0 * 1024.0), D, R, dp.multiProcessorCount, D, R, dp.major, dp.minor, D, R, dp.l2CacheSize / 1024);
	printf("  %sCUDA  %s driver %s%d.%d%s / runtime %s%d.%d%s   %s\xc2\xb7%s   GPUs: %s%d%s\r\n",
		D, R, W, drv / 1000, (drv % 100) / 10, R, W, rt / 1000, (rt % 100) / 10, R, D, R, W, GpuCnt, R);
	if (gRndMode)
		printf("  %sModo  %s %sLOTERIA%s %s(randomico por janelas)%s   %s\xe2\x9a\xa0 a chave so e achada se cair na janela sorteada%s\r\n", D, R, A, R, D, R, A, R);
	else
		printf("  %sModo  %s %sSOLVE DIRETO%s\r\n", D, R, A, R);
	if (gPuzzleNum > 0)
		printf("  %sPuzzle%s %s#%d%s %s\xc2\xb7%s range %s%d bits%s %s\xc2\xb7%s %s%s%s\r\n",
			D, R, W, gPuzzleNum, R, D, R, W, gRange, R, D, R, gPuzzleSolved ? G : A, gPuzzleSolved ? "RESOLVIDO (teste)" : "EM ABERTO", R);
	else
		printf("  %sAlvo  %s range %s%d bits%s\r\n", D, R, W, gRange, R);
	if (gRndMode)
	{
		char ws[100]; gStart.GetHexStr(ws);
		char* p = ws; while (*p == '0' && *(p + 1)) p++; // tira zeros a esquerda
		char ss[48]; int pl = (int)strlen(p);
		if (pl > 14) snprintf(ss, sizeof(ss), "%.6s\xe2\x80\xa6%s", p, p + pl - 4); // 6 + ... + 4
		else snprintf(ss, sizeof(ss), "%s", p);
		printf("  %sJanela%s %s%d bits%s %s\xc2\xb7%s inicio %s0x%s%s\r\n", D, R, W, gWBits, R, D, R, B, ss, R);
	}
	printf("\r\n");
}

int main(int argc, char* argv[])
{
	bool do_clear = true; // limpa a tela ao iniciar (sessao limpa); desliga com -no-clear
	for (int i = 1; i < argc; i++)
		if (strcmp(argv[i], "-no-clear") == 0 || strcmp(argv[i], "--no-clear") == 0) do_clear = false;
	if (do_clear) printf("\033[3J\033[2J\033[H"); // limpa tela visivel + scrollback + cursor no topo

	ui_detect_width(); // detecta a largura real do terminal e preenche a janela toda
#ifdef _DEBUG	
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	ui_rep("*", gUIW); printf("\r\n");
	ui_banner("RCKangaroo BR v3.1  -  base (c) 2024 RetiredCoder  (GPLv3)", "\033[1;36m");
	ui_banner("Versao BR por Tarcisio Soria (a.k.a KKND)", "\033[1;32m");
	ui_rep("*", gUIW); printf("\r\n\r\n");

	printf("\033[2mSoftware livre (GPLv3) | SOTA Kangaroo O(raiz(n)) para o ECDLP | Versao %s | base: github.com/RetiredC\033[0m\r\n",
#ifdef _WIN32
		"Windows"
#else
		"Linux"
#endif
	);

#ifdef DEBUG_MODE
	printf("MODO DEBUG\r\n\r\n");
#endif

	InitEc();
	gDP = 0;
	gRange = 0;
	gStartSet = false;
	gTamesFileName[0] = 0;
	gMax = 0.0;
	gGenMode = false;
	gIsOpsLimit = false;
	gRndMode = false;
	gWBits = 0;
	memset(gGPUs_Mask, 1, sizeof(gGPUs_Mask));
	if (!ParseCommandLine(argc, argv))
		return 0;

	InitGpus();

	if (!GpuCnt)
	{
		printf("Nenhuma GPU suportada detectada, saindo\r\n");
		return 0;
	}

	nvml_tel_init(); // telemetria da GPU (temp/clock/fan)
	print_config();  // bloco de configuracao (GPU, CUDA, modo, puzzle, janela)

	pPntList = (u8*)malloc(MAX_CNT_LIST * GPU_DP_SIZE);
	pPntList2 = (u8*)malloc(MAX_CNT_LIST * GPU_DP_SIZE);
	TotalOps = 0;
	TotalSolved = 0;
	gTotalErrors = 0;
	IsBench = gPubKey.x.IsZero();

	if (gRndMode)
	{
		// cabecalho do modo/janela/aviso agora vem no bloco de configuracao (print_config)
		EcInt pk_found;
		u64 win_cnt = 0;
		bool found = false;
		int idxbits = (int)gRange - gWBits; //bits do indice da janela
		u64 tm_rnd = GetTickCount64();
		u64 rnd_total_ops = 0; //chaves testadas somando todas as janelas
		double total_windows = pow(2.0, (double)idxbits); //total de janelas possiveis
		SetRndSeed(GetTickCount64()); //randomiza o 1o sorteio (senao cai sempre na mesma janela)
		gQuiet = true;    //painel fixo: silencia o output verboso do solve
		printf("\r\n");   //espaco antes do painel (impresso uma vez)
		char barstate[224]; memset(barstate, 0, sizeof(barstate)); //celulas da barra ja testadas (cinza)

		while (1)
		{
			//sorteia uma janela aleatoria dentro do range total
			EcInt widx;
			widx.RndBits(idxbits);
			EcInt winOfs;
			winOfs.Assign(widx);
			winOfs.ShiftLeft(gWBits);
			winOfs.Add(gStart); //inicio absoluto da janela

			win_cnt++;
			char ws[100];
			winOfs.GetHexStr(ws);
			// --- painel de progresso da loteria ---
			double ratio = ((double)widx.data[1] * 18446744073709551616.0 + (double)widx.data[0]) / total_windows;
			if (ratio < 0.0) ratio = 0.0;
			if (ratio > 1.0) ratio = 1.0;
			double cover = 100.0 * (double)win_cnt / total_windows; //% de janelas ja testadas
			u64 el = (GetTickCount64() - tm_rnd) / 1000;
			double per_h = el > 0 ? (double)win_cnt * 3600.0 / (double)el : 0.0;
			u64 dd = el / 86400; int hh = (int)((el % 86400) / 3600); int mm = (int)((el % 3600) / 60); int ssx = (int)(el % 60);
			int bw = gUIW - 14; // nº de celulas da barra
			if (bw < 40) bw = 40;
			if (bw > 210) bw = 210;
			// extensao da janela atual na barra (fracao do range total)
			double f0 = ratio;
			double f1 = ratio + 1.0 / total_windows; if (f1 > 1.0) f1 = 1.0;
			int c0 = (int)(f0 * bw); if (c0 < 0) c0 = 0; if (c0 >= bw) c0 = bw - 1;
			int c1 = (int)(f1 * bw); if (c1 > bw) c1 = bw;
			bool subpixel = (c1 <= c0); // janela menor que 1 celula (puzzles gigantes) -> vira marcador
			if (subpixel) c1 = c0 + 1;
			// monta a barra colorida: verde=janela atual, cinza=ja testadas, apagado=nao testadas
			char bar[2048]; int bi = 0; int curtype = -1;
			for (int b = 0; b < bw; b++) {
				int t = (b >= c0 && b < c1) ? 2 : (barstate[b] ? 1 : 0);
				if (t != curtype) {
					const char* col = (t == 2) ? "\033[1;32m" : (t == 1) ? "\033[90m" : "\033[2m";
					int L = (int)strlen(col); memcpy(bar + bi, col, L); bi += L; curtype = t;
				}
				if (t == 0) bar[bi++] = '.';
				else { bar[bi++] = '\xe2'; bar[bi++] = '\x96'; bar[bi++] = '\x88'; } // bloco solido U+2588
			}
			memcpy(bar + bi, "\033[0m", 4); bi += 4; bar[bi] = 0;
			char titu[96];
			snprintf(titu, sizeof(titu), "RCKangaroo BR - LOTERIA - Puzzle range %d bits", gRange);
			if (win_cnt > 1) printf("\033[5A"); //sobe 5 linhas p/ redesenhar o painel no mesmo lugar
			printf("\r");
			ui_rule(titu);                                                                 //linha 1
			printf("  Janelas: \033[1m%llu\033[0m / \033[1m%s\033[0m      Cobertura: \033[31m%s%%\033[0m      Taxa: \033[1m%.0f\033[0m/h      Tempo: \033[1m%llud %02d:%02d:%02d\033[0m\033[K\r\n",
				win_cnt, ui_num_d(total_windows), ui_pct(cover), per_h, dd, hh, mm, ssx);   //linha 2
			printf("  Range:   [%s]\033[K\r\n", bar);                                       //linha 3 (cores embutidas)
			printf("           \033[2mjanela em %s\033[0m\033[K\r\n", ws);                  //linha 4
			ui_rule(NULL);                                                                 //linha 5
			printf("\033[K"); //limpa a linha de status (sera repintada pelo ShowStats)    //linha 6
			fflush(stdout);

			//desloca a chave publica para a janela: P' = P - winOfs*G
			EcPoint PntOfs = ec.MultiplyG(winOfs);
			PntOfs.y.NegModP();
			EcPoint PntToSolve = ec.AddPoints(gPubKey, PntOfs);

			bool solved = SolvePoint(PntToSolve, gWBits, gDP, &pk_found);
			rnd_total_ops += PntTotalOps; //conta as chaves testadas nesta janela (achou ou nao)
			if (!subpixel) for (int b = c0; b < c1 && b < bw; b++) barstate[b] = 1; //marca janela testada (cinza)
			if (solved)
			{
				pk_found.AddModP(winOfs);
				EcPoint tmp = ec.MultiplyG(pk_found);
				if (tmp.IsEqual(gPubKey))
				{
					found = true;
					break;
				}
				printf("\r\nERRO FATAL: chave achada nesta janela esta incorreta\r\n");
			}
			//se parou por limite de ops, a chave nao estava nesta janela: sorteia outra
			if (!gIsOpsLimit)
			{
				gQuiet = false;
				printf("\r\nERRO FATAL: SolvePoint falhou\r\n");
				goto label_end;
			}
			//janela sem a chave: o contador "Janelas" no painel ja mostra o progresso (sem poluir a tela)
		}
		gQuiet = false; //volta ao normal p/ imprimir o resultado final

		if (found)
		{
			char s[100];
			pk_found.GetHexStr(s);
			printf("\r\n\033[1;32mCHAVE PRIVADA: %s\033[0m\r\n", s);
			u64 tsec = (GetTickCount64() - tm_rnd) / 1000;
			printf("Encontrada apos \033[1m%llu\033[0m janela(s)  |  \033[1m%s\033[0m chaves avaliadas (saltos)  |  tempo total \033[1m%llud %02d:%02d:%02d\033[0m\r\n",
				win_cnt, ui_num(rnd_total_ops), tsec / 86400, (int)((tsec % 86400) / 3600), (int)((tsec % 3600) / 60), (int)(tsec % 60));
			double avg_jump = 1.5 * pow(2.0, gWBits / 2.0 + 3.0); // salto medio de cada operacao
			double dist = (double)rnd_total_ops * avg_jump;       // distancia total percorrida (estimada)
			int dk = (int)(floor(log10(dist)) / 3);
			printf("Distancia percorrida pelos cangurus: ~\033[1;34m%s\033[0m chaves  \033[2m(~2^%.0f \xc2\xb7 %.0f %s)\033[0m\r\n\r\n",
				ui_bignum(dist), log2(dist), dist / pow(10.0, 3.0 * dk), scale_word(dk));
			SaveResult(s, gPubKey, gPuzzleNum, gRange, true, gWBits, win_cnt, rnd_total_ops, -1.0, tsec, dist);
		}
	}
	else
	if (!IsBench && !gGenMode)
	{
		printf("\r\n");
		EcPoint PntToSolve, PntOfs;
		EcInt pk, pk_found;

		PntToSolve = gPubKey;
		if (!gStart.IsZero())
		{
			PntOfs = ec.MultiplyG(gStart);
			PntOfs.y.NegModP();
			PntToSolve = ec.AddPoints(PntToSolve, PntOfs);
		}

		char sx[100], sy[100];
		gPubKey.x.GetHexStr(sx);
		gPubKey.y.GetHexStr(sy);
		printf("\033[2mResolvendo chave publica  X: %.16s\xe2\x80\xa6  Y: %.16s\xe2\x80\xa6\033[0m\r\n", sx, sy);

		gPanelMode = true; gQuiet = true;   //painel fixo: silencia o output verboso do solve
		printf("\r\n");                      //espaco antes do painel (impresso uma vez)
		u64 tm_solve = GetTickCount64();
		bool ok = SolvePoint(PntToSolve, gRange, gDP, &pk_found);
		gPanelMode = false; gQuiet = false;  //volta ao normal p/ o resultado final

		if (!ok)
		{
			if (!gIsOpsLimit)
				printf("\r\nERRO FATAL: SolvePoint falhou\r\n");
			goto label_end;
		}
		pk_found.AddModP(gStart);
		EcPoint tmp = ec.MultiplyG(pk_found);
		if (!tmp.IsEqual(gPubKey))
		{
			printf("\r\nERRO FATAL: SolvePoint achou chave incorreta\r\n");
			goto label_end;
		}
		//happy end - resumo no formato decimal (igual ao modo loteria)
		char s[100];
		pk_found.GetHexStr(s);
		u64 solsec = (GetTickCount64() - tm_solve) / 1000;
		double K = (double)PntTotalOps / pow(2.0, gRange / 2.0);
		double avg_jump = 1.5 * pow(2.0, gRange / 2.0 + 3.0); //salto medio de cada operacao
		double dist = (double)PntTotalOps * avg_jump;         //distancia total percorrida (estimada)
		int dk = (int)(floor(log10(dist)) / 3);
		printf("\r\n\033[1;32m\xe2\x9c\x94 CHAVE ENCONTRADA\033[0m\r\n");
		printf("\033[1;32mCHAVE PRIVADA: %s\033[0m\r\n", s);
		printf("Resolvido em \033[1m1 busca\033[0m  |  \033[1m%s\033[0m chaves avaliadas (saltos)  |  K \033[1m%.2f\033[0m  |  tempo total \033[1m%llud %02d:%02d:%02d\033[0m\r\n",
			ui_num(PntTotalOps), K, solsec / 86400, (int)((solsec % 86400) / 3600), (int)((solsec % 3600) / 60), (int)(solsec % 60));
		printf("Distancia percorrida pelos cangurus: ~\033[1;34m%s\033[0m chaves  \033[2m(~2^%.0f \xc2\xb7 %.0f %s)\033[0m\r\n\r\n",
			ui_bignum(dist), log2(dist), dist / pow(10.0, 3.0 * dk), scale_word(dk));
		SaveResult(s, gPubKey, gPuzzleNum, gRange, false, 0, 0, PntTotalOps, K, solsec, dist);
	}
	else
	{
		if (gGenMode)
			printf("\r\nMODO DE GERACAO DE TAMES\r\n");
		else
			printf("\r\nMODO BENCHMARK (randomico)\r\n");
		//solve points, show K
		while (1)
		{
			EcInt pk, pk_found;
			EcPoint PntToSolve;

			if (!gRange)
				gRange = 78;
			if (!gDP)
				gDP = 16;

			//generate random pk
			pk.RndBits(gRange);
			PntToSolve = ec.MultiplyG(pk);

			if (!SolvePoint(PntToSolve, gRange, gDP, &pk_found))
			{
				if (!gIsOpsLimit)
					printf("ERRO FATAL: SolvePoint falhou\r\n");
				break;
			}
			if (!pk_found.IsEqual(pk))
			{
				printf("ERRO FATAL: a chave encontrada esta errada!\r\n");
				break;
			}
			TotalOps += PntTotalOps;
			TotalSolved++;
			u64 ops_per_pnt = TotalOps / TotalSolved;
			double K = (double)ops_per_pnt / pow(2.0, gRange / 2.0);
			printf("Pontos resolvidos: %d, K medio: %.3f (com overheads de DP e GPU)\r\n", TotalSolved, K);
			//if (TotalSolved >= 100) break; //dbg
		}
	}
label_end:
	for (int i = 0; i < GpuCnt; i++)
		delete GpuKangs[i];
	nvml_tel_shutdown();
	DeInitEc();
	free(pPntList2);
	free(pPntList);
}

