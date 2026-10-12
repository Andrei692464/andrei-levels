// ---------------- Multiplayer: server si sesiune ----------------
static const char* MP_URL  = "https://dkbslssefqvjwcotxcfg.supabase.co";
static const char* MP_KEY  = "sb_publishable_cEnanXvgCQR3DLJDjQ9ytA_u0ZUI2Lv";
static const char* MP_SITE = "https://andrei123459-multiplayer.vercel.app/";

struct MPState {
    bool connected = false;
    std::string accessToken;
    std::string userId;
    std::string username;
    long long expiresAt = 0;
};

static MPState g_mp;
static geode::async::TaskHolder s_authTask;
static geode::async::TaskHolder s_profileTask;
static geode::async::TaskHolder s_presenceTask;
static geode::async::TaskHolder s_pollTask;

static std::string jstr(matjson::Value& j, char const* key) {
    if (j.isObject() && j.contains(key)) {
        auto r = j[key].asString();
        if (r) return r.unwrap();
    }
    return "";
}

static web::WebRequest mpRequest(bool withAuth) {
    auto req = web::WebRequest();
    req.userAgent("Andrei123459Levels");
    req.header("apikey", MP_KEY);
    if (withAuth && !g_mp.accessToken.empty()) {
        req.header("Authorization", "Bearer " + g_mp.accessToken);
    }
    return req;
}

static bool mpApplyTokens(web::WebResponse& res) {
    if (!res.ok()) return false;
    auto jr = res.json();
    if (!jr) return false;
    auto j = jr.unwrap();
    auto at = jstr(j, "access_token");
    if (at.empty()) return false;
    g_mp.accessToken = at;
    auto rt = jstr(j, "refresh_token");
    if (!rt.empty()) Mod::get()->setSavedValue("mp-refresh-token", rt);
    long long expiresIn = 3600;
    if (j.contains("expires_in")) expiresIn = j["expires_in"].asInt().unwrapOr(3600);
    g_mp.expiresAt = static_cast<long long>(std::time(nullptr)) + expiresIn;
    if (j.contains("user")) g_mp.userId = jstr(j["user"], "id");
    return true;
}

// verifica / reinnoieste sesiunea
static void mpEnsureSession(std::function<void(bool)> cb) {
    long long now = static_cast<long long>(std::time(nullptr));
    if (!g_mp.accessToken.empty() && g_mp.expiresAt - now > 60) {
        cb(true);
        return;
    }
    auto rt = Mod::get()->getSavedValue<std::string>("mp-refresh-token", "");
    if (rt.empty()) {
        cb(false);
        return;
    }
    auto req = mpRequest(false);
    req.bodyJSON(matjson::makeObject({ {"refresh_token", rt} }));
    s_authTask.spawn(
        req.post(fmt::format("{}/auth/v1/token?grant_type=refresh_token", MP_URL)),
        [cb](web::WebResponse res) {
            cb(mpApplyTokens(res));
        }
    );
}

// creeaza/ia profilul de jucator (acum primeste username)
static void mpEnsureProfile(std::string username, std::function<void(bool, std::string)> cb) {
    auto req = mpRequest(true);
    req.header("Content-Type", "application/json");
    req.bodyJSON(matjson::makeObject({
        {"username", username}
    }));
    s_profileTask.spawn(
        req.post(fmt::format("{}/rest/v1/rpc/mp_ensure_profile", MP_URL)),
        [cb, username](web::WebResponse res) {
            auto jr = res.json();
            if (!res.ok() || !jr) {
                std::string m = "Eroare server (cod " + std::to_string(res.code()) + ")";
                if (jr) {
                    auto ej = jr.unwrap();
                    auto em = jstr(ej, "message");
                    if (!em.empty()) m = em;
                }
                cb(false, m);
                return;
            }
            auto j = jr.unwrap();
            if (j.isArray()) {
                auto arr = j.asArray();
                if (arr) {
                    auto v = arr.unwrap();
                    if (!v.empty()) j = v[0];
                }
            }
            auto name = jstr(j, "username");
            if (name.empty()) name = username; // fallback
            g_mp.username = name;
            cb(true, name);
        }
    );
}

// ... (mpSendPresence si mpLogout raman la fel)

// ---------------- Fereastra de login (doar username) ----------------
class MultiplayerPopup;

class MultiplayerLoginPopup : public Popup<> {
protected:
    TextInput* m_username = nullptr;
    CCLabelBMFont* m_status = nullptr;
    geode::async::TaskHolder m_task;
    bool m_busy = false;

    bool init() {
        if (!Popup::init(320.f, 200.f, "GJ_square01.png")) return false;
        this->setTitle("Andrei123459 Multiplayer");

        m_username = TextInput::create(260.f, "Nume jucator", "bigFont.fnt");
        m_username->setCommonFilter(CommonFilter::Any);
        m_username->setMaxCharCount(20);
        m_mainLayer->addChildAtPosition(m_username, Anchor::Center, ccp(0, 25));

        m_status = CCLabelBMFont::create("Alege un nume ca sa joci online", "chatFont.fnt");
        m_status->setScale(.7f);
        m_mainLayer->addChildAtPosition(m_status, Anchor::Center, ccp(0, -15));

        auto loginSpr = ButtonSprite::create("Joaca", 140, true, "goldFont.fnt", "GJ_button_01.png", 30.f, .7f);
        auto loginBtn = CCMenuItemSpriteExtra::create(
            loginSpr, this, menu_selector(MultiplayerLoginPopup::onLogin)
        );
        m_buttonMenu->addChildAtPosition(loginBtn, Anchor::Bottom, ccp(0, 30));

        return true;
    }

    void setStatus(std::string text) {
        m_status->setString(text.c_str());
    }

    void onLogin(CCObject*) {
        if (m_busy) return;
        std::string name = std::string(m_username->getString());
        // curatam spatiile
        while (!name.empty() && name.front() == ' ') name.erase(name.begin());
        while (!name.empty() && name.back() == ' ') name.pop_back();

        if (name.size() < 2) {
            this->setStatus("Numele trebuie sa aiba minim 2 caractere.");
            return;
        }
        if (name.size() > 20) {
            this->setStatus("Numele este prea lung (max 20).");
            return;
        }

        m_busy = true;
        this->setStatus("Se conecteaza...");

        // Autentificare anonima
        auto req = mpRequest(false);
        req.header("Content-Type", "application/json");
        req.bodyJSON(matjson::makeObject({
            {"data", matjson::makeObject({})}
        }));

        m_task.spawn(
            req.post(fmt::format("{}/auth/v1/signup", MP_URL)),
            [this, name](web::WebResponse res) {
                if (!mpApplyTokens(res)) {
                    m_busy = false;
                    this->setStatus("Nu ma pot conecta la server. Incearca din nou.");
                    return;
                }

                this->setStatus("Se creeaza profilul...");
                mpEnsureProfile(name, [self = Ref(this)](bool ok, std::string info) {
                    self->m_busy = false;
                    if (!ok) {
                        self->setStatus(info);
                        return;
                    }
                    g_mp.connected = true;
                    self->onClose(nullptr);
                    openMultiplayerPopup();
                });
            }
        );
    }

public:
    static void openMultiplayerPopup();
    static MultiplayerLoginPopup* create() {
        auto ret = new MultiplayerLoginPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ... (MultiplayerPopup ramane aproape la fel)

void MultiplayerLoginPopup::openMultiplayerPopup() {
    if (auto p = MultiplayerPopup::create()) p->show();
}

// apasa pe butonul Multiplayer
static void openMultiplayer() {
    mpEnsureSession([](bool ok) {
        if (!ok) {
            if (auto p = MultiplayerLoginPopup::create()) p->show();
            return;
        }
        // avem deja sesiune → verificam profilul
        auto savedName = Mod::get()->getSavedValue<std::string>("mp-username", "");
        if (savedName.empty()) {
            if (auto p = MultiplayerLoginPopup::create()) p->show();
            return;
        }
        mpEnsureProfile(savedName, [](bool ok2, std::string) {
            if (!ok2) {
                if (auto p = MultiplayerLoginPopup::create()) p->show();
                return;
            }
            g_mp.connected = true;
            MultiplayerLoginPopup::openMultiplayerPopup();
        });
    });
}
