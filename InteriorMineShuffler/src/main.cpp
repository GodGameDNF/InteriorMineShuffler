#include <fstream>
#include <iostream>
#include <windows.h>
#include <string>
#include <vector>

#include <chrono>
#include <ctime>
#include <iomanip>

using namespace RE;

PlayerCharacter* p = nullptr;
BSScript::IVirtualMachine* vm = nullptr;
TESDataHandler* dataHandler = nullptr;

TESGlobal* gMult = nullptr;
TESForm* rMarker = nullptr;
TESObjectCELL* dCell = nullptr;

struct RefrOrInventoryObj
{
	TESObjectREFR* _ref{ nullptr };        // 00
	TESObjectREFR* _container{ nullptr };  // 08
	std::uint16_t _uniqueID{ 0 };          // 10
};

bool MoveToNearestNavmeshLocation(BSScript::IVirtualMachine* vm, uint32_t i, RefrOrInventoryObj* ref)
{
	using func_t = decltype(&MoveToNearestNavmeshLocation);
	REL::Relocation<func_t> func{ REL::ID(620999) };
	return func(vm, i, ref);
}

void SetOwner(TESObjectREFR* target, TESForm* owner)
{
	using func_t = decltype(&SetOwner);
	REL::Relocation<func_t> func{ REL::ID(750649) };
	return func(target, owner);
}

NiPoint3* GetMultiBoundHalfExtent(TESObjectREFR* a)  // Primitive 사이즈 구함. Nipoint3 의 포인터값이 나옴
{
	using func_t = decltype(&GetMultiBoundHalfExtent);
	REL::Relocation<func_t> func{ REL::ID(648171) };
	return func(a);
}

double GetRandomFloat(double a, double b)
{
	std::random_device rd;
	std::mt19937 mt(rd());
	std::uniform_real_distribution<double> dist(a, b);
	double randNum = dist(mt);

	return randNum;
}

bool EnumReferencesCloseToRef(RE::TESDataHandler* SendHandler, RE::TESObjectREFR* TargetRef, float fDistance, RE::NiPoint3* tPoint, float fdistance02, bool(__fastcall*)(RE::TESObjectREFR* refr, void* acc), void* acc2)
{
	using func_t = decltype(&EnumReferencesCloseToRef);
	REL::Relocation<func_t> func{ REL::ID(1348560) };

	return func(
		SendHandler, TargetRef, fDistance, tPoint, fdistance02,
		[](RE::TESObjectREFR* refr, void* acc) -> bool {
			if (refr->formType == ENUM_FORM_ID::kPGRE) {
				NiPointer<TESObjectREFR> oRef = *static_cast<NiPointer<TESObjectREFR>*>(acc);
				if (refr->parentCell != p->parentCell || refr->GetDelete() || refr == oRef.get()) {
					return false;
				} else {
					return true;
				}
			}
		},
		acc2);
}

void shuffleMine(std::monostate)
{
	TESObjectCELL* cell = p->parentCell;
	if (!cell)
		return;

	if (!cell->IsInterior())
		return;

	float checkCount = gMult->value;
	if (checkCount < 1) {
		return;
	} else if(checkCount > 30) {
		checkCount = 30;
	}


	int originCount = 0;

	std::vector<NiPointer<TESObjectREFR>> roomArray;
	std::vector<NiPointer<TESObjectREFR>> mineArray;
	std::vector<NiPointer<TESObjectREFR>> moveTargetArray;

	/// 지뢰와 룸마커를 확인
	BSTArray<NiPointer<TESObjectREFR>> cellRefs = cell->references;
	TESObjectCELL* tCell = p->parentCell;

	for (NiPointer<TESObjectREFR> ref : cellRefs) {
		TESForm* rType = ref->data.objectReference;
		if (ref->formType == ENUM_FORM_ID::kPGRE) {
			if (!ref->IsDeleted() && !ref->IsCreated() && ref->parentCell == tCell) {  // 지뢰가 이미 터졌는지 에디터상으로 놓여있는건지 확인

				std::vector<NiPointer<TESObjectREFR>> tempArray;

				// 생성 글로벌에 소수점이 있으면 확률로 +- 갯수를 정함

				float mountCount = checkCount;
				float fRemain = mountCount - (int)mountCount;
				if (GetRandomFloat(0, 1) < fRemain) {
					mountCount = (int)mountCount + 1;
				} else {
					mountCount = (int)mountCount;
				}

				// 지뢰를 생성하고 플레이어에 바로 폭발하는걸 막기위해 disable. 각 배열에 투입
				for (int i = 0; i < mountCount; i++) {
					ObjectRefHandle tempproj = dataHandler->CreateProjectileAtLocation(ref->data.objectReference, ref->data.location + NiPoint3{ (float)i * 30, (float)i * 30, 0 }, ref->data.angle, ref->parentCell, ref->parentCell->worldSpace);
					tempproj.get()->Disable();
					tempArray.push_back(tempproj.get());
					mineArray.push_back(tempproj.get());
				}

				TESForm* tF = ref->GetOwner();
				if (tF) {
					int lim = tempArray.size();
					for (int i = 0; i < lim; i++) {
						NiPointer<TESObjectREFR> tempproj = tempArray[i];
						SetOwner(tempproj.get(), tF);  // 생성된 지뢰에 원본의 팩션 주입
					}
				}
				ref->MarkAsDeleted();  // 원래 지뢰는 삭제해야 무한 증식 안함. 삭제하고 이동시키기
				ref->parentCell = dCell;
				originCount += 1;
			}
		} else if (rType == rMarker) {
			roomArray.push_back(ref);
		} else if ((rType->formType >= ENUM_FORM_ID::kARMO && rType->formType <= ENUM_FORM_ID::kMISC) || rType->formType == ENUM_FORM_ID::kMSTT || rType->formType == ENUM_FORM_ID::kFURN || rType->formType == ENUM_FORM_ID::kAMMO || rType->formType == ENUM_FORM_ID::kWEAP) {
			moveTargetArray.push_back(ref);
		}
	}

	int roomCount = roomArray.size();
	int mineCount = mineArray.size();

	if (roomCount == 0 || mineCount == 0)
		return;

	// 지뢰를 넣을 방 선택을 위해 룸마커의 Primitive 부피를 체크해서 배열에 저장
	std::vector<double> roomVolumeArray;
	double volumeAll = 0;
	for (int i = 0; i < roomCount; i++) {
		NiPoint3* tempPoint = GetMultiBoundHalfExtent(roomArray[i].get());
		double iTemp = tempPoint->x * tempPoint->y * tempPoint->z / 100;
		volumeAll += iTemp;
		roomVolumeArray.push_back(volumeAll);
	}

	// 랜덤 int 뽑아서 방고르기
	for (int i = 0; i < mineCount; i++) {
		NiPointer<TESObjectREFR> selMine = mineArray[i];
		if (!selMine)
			continue;

		RefrOrInventoryObj* tempObj = new RefrOrInventoryObj;
		tempObj->_ref = selMine.get();

		int iRefeat = 0;
		bool bPlayerNear = true;

		if (GetRandomFloat(0, 1) > 0.7) { // true면 방 중에서 선택. 아니면 바닥의 아이템위치로
			while (bPlayerNear && iRefeat < 10) {
				double iSelect = GetRandomFloat(0, volumeAll);
				int iIndex = 0;

				while (iIndex < roomCount && iSelect > roomVolumeArray[iIndex]) {
					iIndex += 1;
				}

				NiPointer<TESObjectREFR> selRoom = roomArray[iIndex];
				if (!selRoom)
					continue;

				// 방 크기 다시 구하고 각도로 방 범위 확인하고 지뢰 실제로 놓기
				NiPoint3* tempSize = GetMultiBoundHalfExtent(selRoom.get());
				float tX = tempSize->x;
				float tY = tempSize->y;
				float tZ = tempSize->z;

				NiPoint3 roomLocation = selRoom->data.location;
				float z = selRoom->data.angle.z;
				float s = sin(z);
				float c = cos(z);

				// 지뢰를 옮긴후 근처에 플레이어나 다른 지뢰 있는지 확인
				float rX = GetRandomFloat(-tX, tX);
				float rY = GetRandomFloat(-tY, tY);
				float rZ = GetRandomFloat(-tZ, tZ);

				NiPoint3 movePoint = roomLocation + NiPoint3{ rX * s, rY * c, rZ };
				selMine->data.location = (NiPoint3A)movePoint;

				MoveToNearestNavmeshLocation(vm, 0, tempObj);

				float tempDistance = p->data.location.GetDistance(selMine->data.location);
				bPlayerNear = tempDistance <= 200;

				if (!bPlayerNear) {
					bPlayerNear = EnumReferencesCloseToRef(dataHandler, selMine.get(), 150.0, &selMine->data.location, 150.0, nullptr, &selMine);
				}

				iRefeat += 1;
			}
		} else {
			while (bPlayerNear && iRefeat < 10) {
				uint32_t randomInt = GetRandomFloat(0, moveTargetArray.size());
				TESObjectREFR* randomTarget = moveTargetArray[randomInt].get();

				float r01 = GetRandomFloat(40, 350);
				if (GetRandomFloat(0, 1) > 0.5)
					r01 = -r01;
				float r02 = GetRandomFloat(40, 350);
				if (GetRandomFloat(0, 1) > 0.5)
					r02 = -r02;

				selMine->data.location.x == randomTarget->data.location.x + r01;
				selMine->data.location.y == randomTarget->data.location.y + r02;
				selMine->data.location.z == 50;

				MoveToNearestNavmeshLocation(vm, 0, tempObj);

				float tempDistance = p->data.location.GetDistance(selMine->data.location);
				bPlayerNear = tempDistance <= 200;

				iRefeat += 1;
			}
		}

		selMine.get()->Enable(true);
	}

	//logger::info("원래 지뢰 {}개 / 복사한 지뢰 {}개 / 목표타겟 {}개", originCount, mineCount, moveTargetArray.size());
}

void OnF4SEMessage(F4SE::MessagingInterface::Message* msg)
{
	switch (msg->type) {
	case F4SE::MessagingInterface::kGameLoaded:

		p = PlayerCharacter::GetSingleton();
		dataHandler = RE::TESDataHandler::GetSingleton();
		rMarker = dataHandler->LookupForm(0x1F, "Fallout4.esm");
		gMult = (TESGlobal*)dataHandler->LookupForm(0x800, "InteriorMineShuffler.esp");
		dCell = (TESObjectCELL*)dataHandler->LookupForm(0x802, "InteriorMineShuffler.esp");
	}
}

bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* a_vm)
{
	vm = a_vm;

	//REL::IDDatabase::Offset2ID o2i;
	//logger::info("0x0x1409C50: {}", o2i(0x1409C50));

	a_vm->BindNativeMethod("InteriorMineShuffler"sv, "shuffleMine"sv, shuffleMine);

	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
#ifndef NDEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= fmt::format("{}.log", Version::PROJECT);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("Global Log"s, std::move(sink));

#ifndef NDEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::trace);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%^%l%$] %v"s);

	logger::info("{} v{}", Version::PROJECT, Version::NAME);

	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_f4se->IsEditor()) {
		logger::critical("loaded in editor");
		return false;
	}

	const auto ver = a_f4se->RuntimeVersion();
	if (ver < F4SE::RUNTIME_1_10_162) {
		logger::critical("unsupported runtime v{}", ver.string());
		return false;
	}

	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);

	const F4SE::PapyrusInterface* papyrus = F4SE::GetPapyrusInterface();
	if (papyrus)
		papyrus->Register(RegisterPapyrusFunctions);

	const F4SE::MessagingInterface* message = F4SE::GetMessagingInterface();
	if (message)
		message->RegisterListener(OnF4SEMessage);

	return true;
}
