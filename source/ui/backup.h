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
		const string desc;
		bool enable;
		const string path;
	};
	
	vector<OPEntry> bpo {
		{ "", true, "/system_data/priv/mms/app.db" },
		{ "", true, "/system_data/priv/mms/addcont.db" },
		{ "", true, "/system_data/priv/mms/av_content_bg.db" },

		// handled via full directory, secure appcache not found either
//		{ "", true, "/user/system/webkit/secure/appcache/ApplicationCache.db" },
//		{ "", true, "/user/system/webkit/webbrowser/appcache/ApplicationCache.db" },

		{ "", true, "/system_data/savedata" },
		{ "", true, "/user/home" },
		{ "", true, "/user/trophy" },
		{ "", true, "/user/license" },
		{ "", true, "/user/settings" },
		{ "", true, "/user/system/webkit/secure" },
		{ "", true, "/user/system/webkit/webbrowser" },
		{ "", true, "/system_data/priv/home" },
		{ "", true, "/system_data/priv/license" },
//		{ "", true, "/system_data/priv/activation" },	// moved or what?
	};

	OPType opType=None;
	OPEntry* opTarget=nullptr;

	string backupPath = string("/data/ps4-backup");

public:

	void render()
	{
		auto scaling = 1.0f;	// only used for spacing
		u32 resW=VideoOut::Get().Width(), resH=VideoOut::Get().Height();

		if (None!=opType) {
			bool confirmed=false;
			ImGui::OpenPopup("Confirm");
			if (ImGui::BeginPopupModal("Confirm"))
			{
				if (FatalError==opType)
				{
					ImGui::Text("There was an error! (In: %s)", (nullptr==opTarget?"[ALL] operation":opTarget->path.c_str()));
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

		for (const string& path : pathList) {
			if (path.empty()) return false;

			XXH64_hash_t hash = XXH64(path.data(), path.size(), seed);
			string finalPath = backupPath + string("/") + sHx64(hash);	// file or dir ;shrug;

			string src =( _bak ? path : finalPath);
			string dst =(!_bak ? path : finalPath);

			if (existsFile(path)) {
			//	copyFile(src, dst);
				klog("%s, copy file \"%s\" to \"%s\"\n", _bak?"backup":"restore",
					 src.c_str(), dst.c_str());
				continue;
			}

			if (existsDir(path)) {
			//	copyDir(src, dst);
				klog("%s, copy dir \"%s\" to \"%s\"\n", _bak?"backup":"restore",
					 src.c_str(), dst.c_str());
				continue;
			}

			return false;
		}
		return true;
	}





	XXH64_hash_t const seed = 0xBADC0DE;	// *DO NOT CHANGE SEED OR EXISTING FILES WONT BE FOUND*

	inline u32 _m(struct stat* s) const { return ((u32*)s)[2]; }	// *HACK* OO mighty broken

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

};























