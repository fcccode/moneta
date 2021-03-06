#define MEMORY_SUBREGION_FLAG_HEAP 0x1
#define MEMORY_SUBREGION_FLAG_STACK 0x2
#define MEMORY_SUBREGION_FLAG_TEB 0x4
#define MEMORY_SUBREGION_FLAG_DOTNET 0x8
#define MEMORY_SUBREGION_FLAG_BASE_IMAGE 0x10

typedef class Thread;
typedef class MemDump;
typedef class FileBase;
typedef class PeFile;
typedef enum class Signing_t;

namespace Processes {
	typedef class Thread;
	typedef class Process;
}

namespace Memory {
	class Subregion {
	protected:
		const MEMORY_BASIC_INFORMATION* Basic;
		std::vector<Processes::Thread*> Threads;
		uint32_t PrivateSize;
		HANDLE ProcessHandle;
		uint64_t Flags;
	public:
		Subregion(Processes::Process& OwnerProc, const MEMORY_BASIC_INFORMATION* Mbi);
		virtual ~Subregion();
		const MEMORY_BASIC_INFORMATION* GetBasic() const { return this->Basic; }
		std::vector<Processes::Thread*> GetThreads() const { return this->Threads; }
		void SetPrivateSize(uint32_t dwPrivateSize) { this->PrivateSize = dwPrivateSize; }
		uint32_t GetPrivateSize() const { return this->PrivateSize; }
		uint32_t QueryPrivateSize() const;
		uint64_t GetFlags() const { return this->Flags; }
		void SetFlags(uint64_t qwFlags) { this->Flags = qwFlags; }
		static const wchar_t* ProtectSymbol(uint32_t dwProtect);
		static const wchar_t* AttribDesc(const MEMORY_BASIC_INFORMATION* Mbi);
		static const wchar_t* TypeSymbol(uint32_t dwType);
		static const wchar_t* StateSymbol(uint32_t dwState);
		static bool PageExecutable(uint32_t dwProtect);
	};

	class Entity {
	protected:
		std::vector<Subregion*> Subregions;
		const void* StartVa, * EndVa;
		uint32_t EntitySize;
		//MEMORY_REGION_INFORMATION* RegionInfo;
	public:
		enum Type { UNKNOWN, PE_FILE, MAPPED_FILE, PE_SECTION };
		std::vector<Subregion*> GetSubregions() const { return this->Subregions; }
		//MEMORY_REGION_INFORMATION* GetRegionInfo() { return RegionInfo; }
		const void* GetStartVa() const { return this->StartVa; }
		const void* GetEndVa() const { return this->EndVa; }
		uint32_t GetEntitySize() const { return this->EntitySize; }
		bool ContainsFlag(uint64_t qwFlag) const;
		static Entity* Create(Processes::Process& OwnerProc, std::vector<Subregion*> Subregions); // Factory method for derived PE images, mapped files, unknown memory ranges.
		bool Dump(MemDump& DmpCtx) const;
		void SetSubregions(std::vector<Subregion*>);
		bool IsPartiallyExecutable() const;
		virtual ~Entity();
		virtual Entity::Type GetType() = 0;
	};

	class Region : public Entity { // This is essential, since a parameterized constructor of the base entity class is impossible (since it is an abstract base class with a deferred method). new Entity() is impossible for this reason: only derived classes can be initialized.
	public:
		Region(HANDLE hProcess, std::vector<Subregion*> Subregions);
		Entity::Type GetType() { return Entity::Type::UNKNOWN; }
	};

	class MappedFile : virtual public Region { // Virtual inheritance from entity prevents classes derived from entity from having ambiguous/conflicting content.
	protected:
		FileBase* MapFileBase;
	public:
		MappedFile(HANDLE hProcess, std::vector<Subregion*> Subregions, const wchar_t* FilePath, bool bMemStore = false);
		Entity::Type GetType() { return Entity::Type::MAPPED_FILE; }
		FileBase* GetFileBase() const { return this->MapFileBase; }
		virtual ~MappedFile();
	};

	namespace PeVm {
		class Component : virtual public Region {
		public:
			uint8_t* GetDataPe() const { return this->PeData; }
			Component(HANDLE hProcess, std::vector<Subregion*> Subregions, uint8_t* pPeBuf);
		protected:
			 uint8_t* PeData;
		};

		typedef class Section;
		class Body : public MappedFile, public Component {
		protected:
			std::vector<Section*> Sections;
			std::unique_ptr<::PeFile> FilePe;
			Signing_t Signed;
			bool NonExecutableImage;
			bool PartiallyMapped;
			uint32_t ImageSize;
			uint32_t SigningLevel;
			bool Exe;
			bool Dll;
			class PebModule {
			public:
				const uint8_t* GetBase() const { return (const uint8_t*)this->Info.lpBaseOfDll; }
				const uint8_t* GetEntryPoint() const { return (const uint8_t*)this->Info.EntryPoint; }
				std::wstring GetPath() const { return this->Path; }
				std::wstring GetName() const { return this->Name; }
				uint32_t GetSize() const { return this->Info.SizeOfImage; }
				PebModule(HANDLE hProcess, const uint8_t* pModBase);
				bool Exists() const { return (this->Missing ? false : true); }
			protected:
				MODULEINFO Info;
				std::wstring Name;
				std::wstring Path;
				bool Missing;
			} PebMod;
		public:
			Entity::Type GetType() { return Entity::Type::PE_FILE; }
			::PeFile* GetPeFile() const { return this->FilePe.get(); }
			bool IsSigned() const;
			Signing_t GetSisningType() const;
			bool IsNonExecutableImage() const { return this->NonExecutableImage; }
			bool IsPartiallyMapped() const { return this->PartiallyMapped; }
			std::vector<Section*> GetSections() const { return Sections; }
			Section* GetSection(std::string) const;
			PebModule& GetPebModule() { return PebMod; }
			std::vector<Section*> FindOverlapSect(Subregion& Address);
			uint32_t GetImageSize() const { return this->ImageSize; }
			uint32_t GetSigningLevel() const { return this->SigningLevel; }
			Body(Processes::Process& OwnerProc, std::vector<Subregion*> Subregions, const wchar_t* FilePath);
			virtual ~Body();
		};

		class Section : public Component {
		public:
			Section(HANDLE hProcess, std::vector<Subregion*> Subregions, IMAGE_SECTION_HEADER* SectHdr, uint8_t* pPeBuf);
			const IMAGE_SECTION_HEADER* GetHeader() { return &this->Hdr; }
			Entity::Type GetType() { return Entity::Type::PE_SECTION; }
		protected:
			IMAGE_SECTION_HEADER Hdr;
		};
	}
}