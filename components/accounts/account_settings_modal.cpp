#include "account_settings_modal.h"

#include <imgui.h>
#include <string>
#include <algorithm>

#include "../components.h"
#include "../data.h"
#include "../../utils/roblox_api.h"
#include "../../utils/threading.h"
#include "../../utils/main_thread.h"
#include "../../utils/status.h"
#include "../../utils/modal_popup.h"

using namespace ImGui;
using namespace std;

static bool s_show = false;
static int s_accountId = -1;
static char s_descBuf[512] = "";
static int s_gender = 0;
static int s_month = 1, s_day = 1, s_year = 2000;
static char s_passwordBuf[128] = "";
static bool s_loading = false;

void OpenAccountSettings(int accountId)
{
    s_accountId = accountId;
    s_show = true;
    s_loading = true;
    auto it = find_if(g_accounts.begin(), g_accounts.end(), [&](const AccountData &a){return a.id==accountId;});
    if(it == g_accounts.end()) {
        s_loading = false;
        return;
    }
    string cookie = it->cookie;
    Threading::newThread([cookie]() {
        string desc = RobloxApi::getUserDescription(cookie);
        int g = RobloxApi::getUserGender(cookie);
        int m=0,d=0,y=0;
        RobloxApi::getUserBirthdate(cookie,&m,&d,&y);
        MainThread::Post([desc,g,m,d,y]() {
            strncpy(s_descBuf, desc.c_str(), sizeof(s_descBuf)-1);
            s_descBuf[sizeof(s_descBuf)-1]='\0';
            s_gender = g;
            if(m>0) s_month=m;
            if(d>0) s_day=d;
            if(y>0) s_year=y;
            s_loading = false;
        });
    });
}

static void saveChanges(const string &cookie)
{
    string desc = s_descBuf;
    int g = s_gender;
    int m = s_month, d = s_day, y = s_year;
    string pw = s_passwordBuf;
    Threading::newThread([cookie,desc,g,m,d,y,pw]() {
        bool ok1 = RobloxApi::updateUserDescription(cookie, desc);
        bool ok2 = RobloxApi::updateUserGender(cookie, g);
        bool ok3 = pw.empty() ? true : RobloxApi::updateUserBirthdate(cookie,m,d,y,pw);
        MainThread::Post([ok1,ok2,ok3]() {
            if(ok1 && ok2 && ok3)
                Status::Set("Account settings updated");
            else
                Status::Error("Failed to update settings");
            s_show = false;
            s_loading = false;
            s_passwordBuf[0]='\0';
        });
    });
}

void RenderAccountSettingsModal()
{
    if(!s_show) return;
    if(!IsPopupOpen("Account Settings"))
        OpenPopup("Account Settings");
    if(BeginPopupModal("Account Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if(s_loading)
        {
            TextUnformatted("Loading...");
        }
        else
        {
            InputTextMultiline("Description", s_descBuf, IM_ARRAYSIZE(s_descBuf), ImVec2(300,80));
            const char* genderItems[] = {"Unspecified","Male","Female"};
            Combo("Gender", &s_gender, genderItems, IM_ARRAYSIZE(genderItems));
            InputInt("Month", &s_month); 
            InputInt("Day", &s_day);
            InputInt("Year", &s_year);
            InputText("Password", s_passwordBuf, IM_ARRAYSIZE(s_passwordBuf), ImGuiInputTextFlags_Password);
        }
        Spacing();
        if(Button("Save") && !s_loading)
        {
            auto it = find_if(g_accounts.begin(), g_accounts.end(), [&](const AccountData &a){return a.id==s_accountId;});
            if(it!=g_accounts.end()) {
                s_loading = true;
                saveChanges(it->cookie);
            }
        }
        SameLine();
        if(Button("Cancel") && !s_loading)
        {
            s_show = false;
            CloseCurrentPopup();
        }
        EndPopup();
    }
}

