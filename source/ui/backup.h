/*
*	Backup Scene
*/
#pragma once

#define XXH_VECTOR 0 // XXH_SCALAR
#define XXH_INLINE_ALL 1
#include "xxhash.h"

class BackupUI
{
	enum OPType {
		None=0,
		Backup,
		Restore,
		BackupALL,
		RestoreALL,
		FatalError,
		OPType_Count
	};

	struct OPEntry {
		bool enable;
		const string desc;	// Overrides UI display string
		const string path;
	};
	
	vector<OPEntry> bpo {	// !! use directories,  not single files atm !!
		{ true, "", "/system_data/priv/mms" },
		{ true, "", "/system_data/savedata" },
		{ true, "", "/user/home" },
		{ true, "", "/user/trophy" },
		{ true, "", "/user/license" },
		{ true, "", "/user/settings" },
		{ true, "", "/user/system/webkit/secure" },
		{ true, "", "/user/system/webkit/webbrowser" },
		{ true, "", "/system_data/priv/home" },
		{ true, "", "/system_data/priv/license" },
//		{ true, "", "/system_data/priv/activation" },	// moved or what?
	};

	OPType opType=None;
	OPEntry* opTarget=nullptr;

	string backupPath = string("/data/ps4-backup");
	string errorStr;
public:

	void render()
	{
		auto scaling = 1.0f;	// only used for spacing
		u32 resW=VideoOut::Get().Width(), resH=VideoOut::Get().Height();

		if (None!=opType) {
			bool confirmed=false;
			ImGui::OpenPopup("Confirm");

			ImGui::SetNextWindowSize(ImVec2(600, 400));
			if (ImGui::BeginPopupModal("Confirm"))
			{
				if (FatalError==opType)
				{
					ImGui::Text("There was an error! (In: %s)", (nullptr==opTarget?"[ALL] operation":opTarget->path.c_str()));
					if(!errorStr.empty())
						ImGui::Text("%s", errorStr.c_str());

					if (ImGui::Button("OK"))
						confirmed=true;
				}
				else {
					ImGui::Text("Are you certain?");
					if (ImGui::Button("YES")) {
						if (!justDoIT())
							opType=FatalError;
						else 
							confirmed=true;
					}

					ImGui::SameLine(44);
					if (ImGui::Button("No... scared: lemme out of here!")) 
						confirmed=true;
				}

				ImGui::EndPopup();
			}

			if(confirmed) {
				opType=None;
				opTarget=nullptr;
				errorStr.clear();
			}
		}

		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(ImVec2(resW, resH));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

		ImGui::Begin("##backup", NULL, ImGuiWindowFlags_NoDecoration);


		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(22 * scaling, 10 * scaling));		// from 8, 4
		ImGui::AlignTextToFramePadding();
		ImGui::Text("\t*-\tps4-backup\t-* Use buttons to backup or restore single items or ALL items");
		ImGui::Text(" ");
		ImGui::Text(" ");

#if _FULLSCREEN && _SETTINGS
		ImGui::SameLine(ImGui::GetContentRegionAvailWidth() - ImGui::CalcTextSize("Settings").x - ImGui::GetStyle().FramePadding.x * 2.0f /*+ ImGui::GetStyle().ItemSpacing.x*/);
		if (ImGui::Button("Settings"));
#endif
		
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8 * scaling, 20 * scaling));		// from 8, 4

			for (auto& e : bpo)
			{
				ImGui::PushID(&e);

				if (ImGui::Button("Backup")) {
					opType=Backup;
					opTarget=&e;
				}

				ImGui::SameLine(110);
				if (ImGui::Button("Restore")) {
					opType=Restore;
					opTarget=&e;
				}

				const string& s = (e.desc.empty() ? e.path : e.desc);

				ImGui::SameLine(230.f);
				bool itBeChecked=e.enable;
				if(ImGui::Checkbox("[ALL] Enable for global Backup/Restore", &itBeChecked)) {
					e.enable=itBeChecked;
					klog("entry %s is now %s\n", s.c_str(), e.enable?"true":"false");
				}

				ImGui::SameLine(550.f);
				ImGui::Text("\"  %s  \"", s.c_str());

				ImGui::PopID();
			}

			ImGui::PopStyleVar();
		}
		ImGui::Text(" ");
		ImGui::Text(" ");
		if (ImGui::Button("Backup [ALL]")) {
			opType=BackupALL;
		}

		ImGui::SameLine(160);
		if (ImGui::Button("Restore [ALL]"))
			opType=RestoreALL;


		ImGui::PopStyleVar();
		ImGui::PopStyleVar();

		ImGui::End();

	}



private:


	// could spawn thread and check mtx locked var or atomic
	bool justDoIT()
	{
		vector<string> pathList;

		if ((None==opType || FatalError==opType) ||
			((Backup==opType || Restore==opType) && !opTarget))
			return false;

		if (Backup==opType || Restore==opType)
			pathList.push_back(opTarget->path);
		else {

			for (auto& e : bpo)
				if (e.enable)
					pathList.push_back(e.path);
		}

		bool _bak=false;
		if (Backup==opType || BackupALL==opType) _bak=true;

		if (!existsDir(backupPath)) {
			if(!_bak) { errorStr=string("path must exist for restore!"); return false; }

			if(-1 == mkdir(backupPath.c_str(),0777) &&	EEXIST != errno) {
				klog("justDoIT(backupPath: \"%s\") mkdir() failed with 0x%08X\n", backupPath.c_str(), errno);
				return false;
			}
		}

		for (const string& path : pathList) {
			if (path.empty()) return false;

			XXH64_hash_t hash = XXH64(path.data(), path.size(), seed);
			string finalPath = backupPath + string("/") + sHx64(hash);	// file or dir ;shrug;

#if 0	// if you use single files in list , there is annoyance here
			if (!_bak) {
				size_t po = 0;
				if (string::npos == (po=path.rfind('/'))) {
					errorStr=string("justDoIt() failed to find restore single file entry string cut, will fail!");
					return false;
				}
				else if (po<path.size()) po++;

				finalPath += string("/") + path.substr(po);
			}
#endif

			string src =( _bak ? path : finalPath);
			string dst =(!_bak ? path : finalPath);

			if (existsFile(path)) {
#if _DEBUG
				klog("%s\n", string(80,'@').c_str());
				klog("%s, copy file \"%s\" to \"%s\"\n", _bak?"backup":"restore",
					 src.c_str(), dst.c_str());
				klog("%s\n", string(80,'*').c_str());
#endif
				if(!copyFile(src, dst))
					return false;
			}

			else if (existsDir(path)) {
#if _DEBUG
				klog("%s\n", string(80,'@').c_str());
				klog("%s, copy dir \"%s\" to \"%s\"\n", _bak?"backup":"restore",
					 src.c_str(), dst.c_str());
				klog("%s\n", string(80,'*').c_str());
#endif
				if(!copyDir(src, dst))
					return false;
			}

			else return false;
		}
		return true;
	}





	XXH64_hash_t const seed = 0xBADC0DE;	// *DO NOT CHANGE SEED OR EXISTING FILES WONT BE FOUND*

	inline static u32 _m(const struct stat* s) { return ((u32*)s)[2]; }	// *HACK* OO mighty broken

	inline bool existsDir(const char* sPath) {
		struct stat sb;	return (stat(sPath, &sb) == 0 && S_ISDIR(_m(&sb)));
	}
	inline bool existsFile(const char* sPath) {
		struct stat sb;	return (stat(sPath, &sb) == 0 && S_ISREG(_m(&sb)));
	}

	inline bool existsDir (const string& path) { return existsDir (path.c_str()); }
	inline bool existsFile(const string& path) { return existsFile(path.c_str()); }

	inline string sHx64(uint64_t v) {
		thread_local static char b[17];
		b[16]=0;
		sprintf(b,"%016lX",v);
		return string(b);	//ltoa(v,b,16));	// no ltoa ...
	}





	bool copyFile(const string& src, const string& dst)
	{
		klog("copyFile(src: \"%s\", dst \"%s\") \n", src.c_str(), dst.c_str());
		if(!existsFile(src))
			return false;

		off_t size=0, actual=0;
		vector<u8> fileBuff;

		int fd = 0;
		if ((fd=open(src.c_str(), O_RDONLY)) <= 0) {
			klog("open(src): got fd %d , errno 0x%08X\n", fd, errno);
			if (EACCES==errno)
				errorStr=string("Access Denied !");

			return false;
		}
		size = lseek(fd, 0, SEEK_END);
		klog("-src size: %ld bytes!\n", size);
		lseek(fd, 0, SEEK_SET);
		if (size <= 0)
			return false;
			
		fileBuff.resize(size);
		if (size!=(actual=read(fd, &fileBuff[0], size))) {
			klog("@@ bad read , read %ld / %ld bytes : errno: 0x%08X\n", actual, size, errno);
			return false;
		}

		close(fd);


		int ps = 0774;
		//mode=_p(stat(dst))
		if ((fd=open(dst.c_str(), O_WRONLY|O_CREAT, ps)) <= 0) {
			klog("open(dst): got fd %d , errno 0x%08X\n", fd, errno);
			if (EACCES==errno)
				errorStr=string("Access Denied !");

			return false;
		}
		if (size!=(actual=write(fd, &fileBuff[0], size))) {
			klog("@@ bad write , wrote %ld / %ld bytes : errno: 0x%08X\n", actual, size, errno);
			return false;
		}
		close(fd);


		return true;
	}



	bool copyDir(const string& src, const string& dst)
	{
		klog("copyDir(src: \"%s\", dst \"%s\") \n", src.c_str(), dst.c_str());
		if(!existsDir(src))
			return false;

#if 0	// ofc libc is broken,  it was built with broken structs, thX OO
		int res=0;
		if (0 != (res=nftw(src.c_str(), dirEntFTW, 20, 0))) { // FTW_CHDIR|FTW_MOUNT
			klog("nftw() res: 0x%08X\n",res);
			return false;
		}
#else	// we shall get next level stupid with it instead

		vector<string> entList;

		if(!getEntries(src, entList))
			return false;

		{
			// perms sbo for backup, have to exist for restore
			if(!!mkpath(dst,0774)) {	//if(-1 == mkdir(dst.c_str(),0774) &&	EEXIST != errno) {
				klog("copyDir(dst: \"%s\") mkdir() failed with 0x%08X\n", dst.c_str(), errno);
				return false;
			}
			for (const auto& e : entList)
			{
				if (string(".")==e || string("..")==e)
					continue;

				string fullSrc = src + string("/") + e;
				string fullDst = dst + string("/") + e;
				if (existsFile(fullSrc)) {
				//	klog("copyDir(src: \"%s\") entry \"%s\" rec copyFile()\n", src.c_str(), e.c_str());
					if(!copyFile(fullSrc, fullDst))
						return false;
				}
				else if (existsDir(fullSrc)) {
				//	klog("copyDir(src: \"%s\") entry \"%s\" rec copyDir()\n", src.c_str(), e.c_str());
					if(!copyDir(fullSrc, fullDst))
						return false;
				}
				else klog("copyDir(src: \"%s\") entry \"%s\" is something other than file/dir!\n",src.c_str(), e.c_str());
			}
		}


#endif

		return true;
	}




#if 0	// someday
	static int dirEntFTW(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf)
	{
		klog("%s(fpath: \"%s\", stat.mode: 0x%08X)\n",__func__,fpath,_m(sb));
		return 0;
	}
#endif



	bool getEntries(const string& dir, vector<string>& entries)
	{
		static vector<char> dentBuff(0x10000,0);
		entries.clear();

		if (dir.empty())
			return false;

#ifdef _DEBUG
		klog("%s(\"%s\") \n", __func__, dir.c_str());
#endif
		int dfd = open(dir.c_str(), O_RDONLY | O_DIRECTORY);
		if (dfd > 0) {

			dentBuff.resize(0x10000);	// 64k ought to be enough, really... 

			int dsize = 0;
			while (0 < (dsize = sceKernelGetdents(dfd, &dentBuff[0], dentBuff.size())))
			{
				int offs = 0;
				dirent *de = (dirent *)&dentBuff[offs];

				while (offs < dsize && de) {
					de = (dirent *)&dentBuff[offs];
#ifdef _DEBUG
					klog("entry fileNo: %X , name: \"%s\" \n", de->d_fileno, de->d_name);
#endif
					entries.push_back(de->d_name);
					offs += de->d_reclen;
				}
			}

			close(dfd);
			return true;
		}

		return false;
	}

	static int mkpath(const std::string& s, mode_t mode)
	{
		size_t pos=0;
		std::string dir, tmp=s;

		if(tmp[tmp.size()-1]!='/'){
			tmp+=string("/");	// force trailing / so we can handle everything in loop
		}

		while((pos=tmp.find_first_of('/',pos))!=std::string::npos){
			dir=tmp.substr(0,pos++);
			if(dir.size()==0) continue; // if leading / first time is 0 length
			if(-1==mkdir(dir.c_str(),mode) && errno!=EEXIST){
				return errno;
			}
		}
		return 0;
	}


	static bool mkpath(const char *dir, u32 p=0774)
	{
		return mkpath(string(dir),mode_t(p));
	}



};























