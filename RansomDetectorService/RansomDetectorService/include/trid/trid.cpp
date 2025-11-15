
#ifdef _M_IX86
#include "trid.h"
#include "../ulti/include.h"
#include "../ulti/support.h"
#include "../ulti/debug.h"

namespace type_iden
{
	TrID::~TrID()
	{
		if (trid_api_ != nullptr)
		{
			delete trid_api_;
			trid_api_ = nullptr;
		}
	}

	bool TrID::Init(const std::wstring& defs_dir, const std::wstring& trid_dll_path)
	{
		std::lock_guard<std::mutex> lock(m_mutex_);
		if (ulti::IsCurrentX86Process() == false)
		{
			PrintDebugW(L"Current process is not a x86 process");
			return false;
		}
		if (trid_api_ != nullptr)
		{
			delete trid_api_;
			trid_api_ = nullptr;
		}
		try
		{
			trid_api_ = new TridApi(ulti::WstrToStr(defs_dir).c_str(), trid_dll_path.c_str());
		}
		catch (int trid_error_code)
		{
			if (trid_error_code == TRID_MISSING_LIBRARY)
			{
				PrintDebugW(L"TrIDLib.dll not found");
			}
			if (trid_api_ != nullptr)
			{
				delete trid_api_;
				trid_api_ = nullptr;
			}
			return false;
		}
		return true;
	}

	std::vector<std::string> TrID::GetTypes(const std::wstring& file_path)
	{
		std::lock_guard<std::mutex> lock(m_mutex_); // TRID is not thread safe
		//PrintDebugW(L"Getting types of file: %ws", file_path.wstring().c_str());

		if (trid_api_ == nullptr)
		{
			PrintDebugW(L"TrID API is not initialized");
			return {};
		}
		if (file_path.empty())
		{
			PrintDebugW(L"File path is empty");
			return {};
		}
		int ret;
		std::vector<std::string> types;

		const auto file_path_str = ulti::WstrToStr(file_path);

		char buf[256]; // Buffer for TrID API results
		try
		{
			trid_api_->SubmitFileA(file_path_str.c_str());
			ret = trid_api_->Analyze();
			if (ret)
			{
				ZeroMemory(buf, sizeof(buf)); // Clear buffer for next use
				//PrintDebugW(L"TrID analysis successful for file %ws, result code: %d", file_path.c_str(), ret);
				ret = trid_api_->GetInfo(TRID_GET_RES_NUM, 0, buf);
				for (int i = ret + 1; i--;)
				{
					ZeroMemory(buf, sizeof(buf)); // Clear buffer for next use

					try { trid_api_->GetInfo(TRID_GET_RES_FILETYPE, i, buf); }
					catch (...) { continue; }

					std::string type_str(buf);
					if (type_str.size() == 0
						|| type_str.find("ransom") != std::string::npos
						|| type_str.find("ncrypt") != std::string::npos)
						continue;

					ZeroMemory(buf, sizeof(buf)); // Clear buffer for next use

					try { trid_api_->GetInfo(TRID_GET_RES_FILEEXT, i, buf); }
					catch (...) { continue; }

					std::string ext_str(buf);

					if (ext_str.size() > 0)
					{
						ulti::ToLowerOverride(ext_str);
						std::stringstream ss(ext_str);
						std::string ext;
						while (std::getline(ss, ext, '/'))
						{
							types.push_back(ext); // Save found extension
						}
					}
					else if (type_str.size() > 0)
					{
						ulti::ToLowerOverride(type_str);
						types.push_back(type_str);
					}
				}
			}
		}
		catch (...)
		{

		}

		return types;
	}
}

#endif // _M_IX86
