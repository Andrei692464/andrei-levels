#include <Geode/Geode.hpp>
#include <Geode/modify/CreatorLayer.hpp>
#include <Geode/modify/LevelSearchLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/ui/TextInput.hpp>
#include <functional>
#include <unordered_map>
#include <ctime>
#include <Geode/ui/Popup.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/loader/Dirs.hpp>
#include <Geode/utils/file.hpp>

using namespace geode::prelude;

// ---------------- Update din joc ----------------
static const char* UPDATE_INFO_URL = "https://api.github.com/repos/Andrei692464/andrei-levels/releases/latest";
static const char* UPDATE_FILE_URL = "https://github.com/Andrei692464/andrei-levels/releases/latest/download/andrei123459.levels.geode";

static geode::async::TaskHolder<web::WebResponse> s_checkTask;
static geode::async::TaskHolder<web::WebResponse> s_downloadTask;
static bool s_checkedThisSession = false;

static void downloadUpdate(std::string version) {
    s_downloadTask.spawn(
        web::WebRequest().userAgent("Andrei123459Levels").get(UPDATE_FILE_URL),
        [version](web::WebResponse res) {
            if (!res.ok() || res.data().size() < 1000) {
                FLAlertLayer::create(
                    "Update esuat",
                    fmt::format("Descarcarea a esuat (cod {}). Incearca din nou mai tarziu.", res.code()),
                    "OK"
                )->show();
                return;
            }

            auto path = dirs::getModsDir() / "andrei123459.levels.geode";
            auto ok = file::writeBinary(path, res.data());
            if (!ok) {
                FLAlertLayer::create(
                    "Update esuat",
                    fmt::format("Nu am putut salva fisierul: {}", ok.unwrapErr()),
                    "OK"
                )->show();
                return;
            }

            FLAlertLayer::create(
                "Update descarcat!",
                fmt::format("Versiunea <cy>{}</c> a fost instalata.\n<cg>Inchide complet jocul si porneste-l din nou</c> ca sa o folosesti.", version),
                "OK"
            )->show();
        }
    );
}

static void checkForUpdate() {
    if (s_checkedThisSession) return;
    s_checkedThisSession = true;

    s_checkTask.spawn(
        web::WebRequest().userAgent("Andrei123459Levels").get(UPDATE_INFO_URL),
        [](web::WebResponse res) {
            if (!res.ok()) return;
            auto json = res.json();
            if (!json) return;
            auto value = json.unwrap();
            auto name = value["name"].asString().unwrapOr("");
            auto latest = VersionInfo::parse(name);
            if (!latest) return;
            auto newVersion = latest.unwrap();
            if (!(newVersion > Mod::get()->getVersion())) return;

            auto versionText = newVersion.toVString();
            createQuickPopup(
                "Update disponibil",
                fmt::format(
                    "O versiune noua a modului <cy>Andrei123459 Levels</c> este disponibila: <cg>{}</c> (acum ai {}).\nVrei sa o descarci?",
                    versionText, Mod::get()->getVersion().toVString()
                ),
                "Mai tarziu", "Update",
                [versionText](FLAlertLayer*, bool update) {
                    if (update) downloadUpdate(versionText);
                }
            );
        }
    );
}

// ---------------- Multiplayer: server si sesiune ----------------
static const char* MP_URL  = "https://dkbslssefqvjwcotxcfg.supabase.co";
static const char* MP_KEY  = "sb_publishable_cEnanXvgCQR3DLJDjQ9ytA_u0ZUI2Lv";
static const char* MP_SITE = "https://andrei123459-multiplayer.vercel.app/";

struct MPState {
    bool connected = false;       // conectat la servere in aceasta sesiune de joc
    std::string accessToken;
    std::string userId;
    std::string username;
    long long expiresAt = 0;
};
static MPState g_mp;

static geode::async::TaskHolder<web::WebResponse> s_authTask;
static geode::async::TaskHolder<web::WebResponse> s_profileTask;
static geode::async::TaskHolder<web::WebResponse> s_presenceTask;
static geode::async::TaskHolder<web::WebResponse> s_pollTask;

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
    if (!rt.empty()) Mod::get()->setSavedValue<std::string>("mp-refresh-token", rt);
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

// creeaza/ia profilul de jucator (numele ales pe site)
static void mpEnsureProfile(std::function<void(bool, std::string)> cb) {
    auto req = mpRequest(true);
    req.header("Content-Type", "application/json");
    req.bodyString("{}");
    s_profileTask.spawn(
        req.post(fmt::format("{}/rest/v1/rpc/mp_ensure_profile", MP_URL)),
        [cb](web::WebResponse res) {
            auto jr = res.json();
            if (!res.ok() || !jr) {
                std::string m = "Eroare server (cod " + std::to_string(res.code()) + ")";
                if (jr) {
                    auto ej = jr.unwrap();
                    auto em = jstr(ej, "message");
                    if (em.find("username required") != std::string::npos) {
                        m = "Alege mai intai un nume de jucator pe site.";
                    } else if (!em.empty()) {
                        m = em;
                    }
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
            if (name.empty()) {
                cb(false, "Profil lipsa. Intra pe site si alege un nume.");
                return;
            }
            g_mp.username = name;
            cb(true, name);
        }
    );
}

// trimite pozitia / prezenta
static void mpSendPresence(int levelId, std::string levelName, float x, float y, float rot) {
    if (!g_mp.connected) return;
    mpEnsureSession([=](bool ok) {
        if (!ok || g_mp.userId.empty()) return;
        auto req = mpRequest(true);
        req.header("Prefer", "resolution=merge-duplicates,return=minimal");
        req.bodyJSON(matjson::makeObject({
            {"user_id", g_mp.userId},
            {"level_id", levelId > 0 ? matjson::Value(levelId) : matjson::Value()},
            {"level_name", levelId > 0 ? matjson::Value(levelName) : matjson::Value()},
            {"x", static_cast<double>(x)},
            {"y", static_cast<double>(y)},
            {"rot", static_cast<double>(rot)},
            {"icon_id", GameManager::sharedState()->getPlayerFrame()}
        }));
        s_presenceTask.spawn(
            req.post(fmt::format("{}/rest/v1/mp_presence?on_conflict=user_id", MP_URL)),
            [](web::WebResponse) {}
        );
    });
}

// deconectare: sterge prezenta si sesiunea
static void mpLogout() {
    if (!g_mp.accessToken.empty() && !g_mp.userId.empty()) {
        auto req = mpRequest(true);
        s_presenceTask.spawn(
            req.send("DELETE", fmt::format("{}/rest/v1/mp_presence?user_id=eq.{}", MP_URL, g_mp.userId)),
            [](web::WebResponse) {}
        );
    }
    g_mp = MPState{};
    Mod::get()->setSavedValue<std::string>("mp-refresh-token", "");
}

// ---------------- Fereastra de login ----------------
class MultiplayerPopup;

class MultiplayerLoginPopup : public Popup {
protected:
    TextInput* m_email = nullptr;
    TextInput* m_pass = nullptr;
    CCLabelBMFont* m_status = nullptr;
    geode::async::TaskHolder<web::WebResponse> m_task;
    bool m_busy = false;

    bool init() {
        if (!Popup::init(330.f, 250.f, "GJ_square01.png")) return false;
        this->setTitle("Andrei123459 Multiplayer");

        m_email = TextInput::create(260.f, "Email", "bigFont.fnt");
        m_email->setCommonFilter(CommonFilter::Any);
        m_email->setMaxCharCount(80);
        m_mainLayer->addChildAtPosition(m_email, Anchor::Center, ccp(0, 40));

        m_pass = TextInput::create(260.f, "Parola", "bigFont.fnt");
        m_pass->setCommonFilter(CommonFilter::Any);
        m_pass->setPasswordMode(true);
        m_pass->setMaxCharCount(80);
        m_mainLayer->addChildAtPosition(m_pass, Anchor::Center, ccp(0, 0));

        m_status = CCLabelBMFont::create("Conecteaza-te cu contul facut pe site", "chatFont.fnt");
        m_status->setScale(.7f);
        m_mainLayer->addChildAtPosition(m_status, Anchor::Center, ccp(0, -35));

        auto loginSpr = ButtonSprite::create("Conecteaza-te", 130, true, "goldFont.fnt", "GJ_button_01.png", 30.f, .7f);
        auto loginBtn = CCMenuItemSpriteExtra::create(
            loginSpr, this, menu_selector(MultiplayerLoginPopup::onLogin)
        );
        m_buttonMenu->addChildAtPosition(loginBtn, Anchor::Bottom, ccp(-72, 34));

        auto siteSpr = ButtonSprite::create("Creeaza cont", 130, true, "goldFont.fnt", "GJ_button_01.png", 30.f, .7f);
        auto siteBtn = CCMenuItemSpriteExtra::create(
            siteSpr, this, menu_selector(MultiplayerLoginPopup::onSite)
        );
        m_buttonMenu->addChildAtPosition(siteBtn, Anchor::Bottom, ccp(72, 34));

        return true;
    }

    void setStatus(std::string text) {
        m_status->setString(text.c_str());
    }

    void onSite(CCObject*) {
        web::openLinkInBrowser(MP_SITE);
    }

    void onLogin(CCObject*) {
        if (m_busy) return;
        std::string email = std::string(m_email->getString());
        std::string pass = std::string(m_pass->getString());
        if (email.empty() || pass.empty()) {
            this->setStatus("Scrie emailul si parola.");
            return;
        }
        m_busy = true;
        this->setStatus("Se conecteaza...");

        auto req = mpRequest(false);
        req.bodyJSON(matjson::makeObject({ {"email", email}, {"password", pass} }));
        m_task.spawn(
            req.post(fmt::format("{}/auth/v1/token?grant_type=password", MP_URL)),
            [this](web::WebResponse res) {
                if (!mpApplyTokens(res)) {
                    m_busy = false;
                    std::string m = "Email sau parola gresite.";
                    auto jr = res.json();
                    if (jr) {
                        auto ej = jr.unwrap();
                        auto em = jstr(ej, "msg");
                        if (em.find("not confirmed") != std::string::npos) {
                            m = "Confirma emailul (link-ul primit pe email).";
                        }
                    } else {
                        m = "Nu ma pot conecta la server.";
                    }
                    this->setStatus(m);
                    return;
                }
                this->setStatus("Se incarca profilul...");
                mpEnsureProfile([self = Ref(this)](bool ok, std::string info) {
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

// ---------------- Fereastra Multiplayer (lista jucatori) ----------------
class MultiplayerPopup : public Popup {
protected:
    CCNode* m_listNode = nullptr;
    geode::async::TaskHolder<web::WebResponse> m_task;

    bool init() {
        if (!Popup::init(340.f, 270.f, "GJ_square01.png")) return false;
        this->setTitle("Andrei123459 Multiplayer");

        auto who = CCLabelBMFont::create(fmt::format("Conectat ca {}", g_mp.username).c_str(), "goldFont.fnt");
        who->setScale(.6f);
        m_mainLayer->addChildAtPosition(who, Anchor::Top, ccp(0, -50));

        auto sub = CCLabelBMFont::create("Jucatori online:", "bigFont.fnt");
        sub->setScale(.4f);
        m_mainLayer->addChildAtPosition(sub, Anchor::Top, ccp(0, -75));

        m_listNode = CCNode::create();
        m_mainLayer->addChildAtPosition(m_listNode, Anchor::Top, ccp(0, -100));

        auto siteSpr = ButtonSprite::create("Site", 100, true, "goldFont.fnt", "GJ_button_01.png", 30.f, .7f);
        auto siteBtn = CCMenuItemSpriteExtra::create(
            siteSpr, this, menu_selector(MultiplayerPopup::onSite)
        );
        m_buttonMenu->addChildAtPosition(siteBtn, Anchor::Bottom, ccp(-70, 30));

        auto outSpr = ButtonSprite::create("Deconectare", 130, true, "goldFont.fnt", "GJ_button_01.png", 30.f, .7f);
        auto outBtn = CCMenuItemSpriteExtra::create(
            outSpr, this, menu_selector(MultiplayerPopup::onLogout)
        );
        m_buttonMenu->addChildAtPosition(outBtn, Anchor::Bottom, ccp(70, 30));

        this->schedule(schedule_selector(MultiplayerPopup::refresh), 4.f);
        this->refresh(0.f);
        return true;
    }

    void onSite(CCObject*) {
        web::openLinkInBrowser(MP_SITE);
    }

    void onLogout(CCObject*) {
        mpLogout();
        this->onClose(nullptr);
    }

    void showLine(std::string text, int index) {
        auto label = CCLabelBMFont::create(text.c_str(), "chatFont.fnt");
        label->setScale(.8f);
        label->setPosition({0.f, -static_cast<float>(index) * 18.f});
        m_listNode->addChild(label);
    }

    void refresh(float) {
        // semnal "sunt online" (in meniu)
        mpSendPresence(0, "", 0.f, 0.f, 0.f);

        mpEnsureSession([self = Ref(this)](bool ok) {
            if (!ok) {
                self->m_listNode->removeAllChildren();
                self->showLine("Sesiune expirata. Conecteaza-te din nou.", 0);
                return;
            }
            self->fetchList();
        });
    }

    void fetchList() {
        auto req = mpRequest(true);
        m_task.spawn(
            req.get(fmt::format(
                "{}/rest/v1/mp_online?select=username,level_name,level_id&order=username.asc&limit=50", MP_URL
            )),
            [this](web::WebResponse res) {
                m_listNode->removeAllChildren();
                auto jr = res.json();
                if (!res.ok() || !jr) {
                    this->showLine("Nu ma pot conecta la server.", 0);
                    return;
                }
                auto j = jr.unwrap();
                auto arr = j.asArray();
                if (!arr || arr.unwrap().empty()) {
                    this->showLine("Nimeni online acum.", 0);
                    return;
                }
                auto items = arr.unwrap();
                int shown = 0;
                for (auto& it : items) {
                    if (shown >= 8) break;
                    std::string name = jstr(it, "username");
                    std::string lvl = jstr(it, "level_name");
                    std::string line = name;
                    if (name == g_mp.username) line += " (tu)";
                    line += lvl.empty() ? "  -  in meniu" : ("  -  joaca: " + lvl);
                    if (line.size() > 46) line = line.substr(0, 43) + "...";
                    this->showLine(line, shown);
                    shown++;
                }
            }
        );
    }

public:
    static MultiplayerPopup* create() {
        auto ret = new MultiplayerPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

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
        mpEnsureProfile([](bool ok2, std::string) {
            if (!ok2) {
                if (auto p = MultiplayerLoginPopup::create()) p->show();
                return;
            }
            g_mp.connected = true;
            MultiplayerLoginPopup::openMultiplayerPopup();
        });
    });
}

// ---------------- Jucatori in nivel ----------------
struct GhostData {
    Ref<CCNode> node;
    CCPoint target;
};

class $modify(AndreiPlayLayer, PlayLayer) {
    struct Fields {
        float sendT = 0.f;
        float pollT = 0.f;
        std::unordered_map<std::string, GhostData> ghosts;
    };

    void clearGhosts() {
        for (auto& kv : m_fields->ghosts) {
            if (kv.second.node) kv.second.node->removeFromParent();
        }
        m_fields->ghosts.clear();
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        if (!g_mp.connected || g_mp.accessToken.empty() || !m_level || !m_player1) {
            if (!m_fields->ghosts.empty()) this->clearGhosts();
            return;
        }
        int levelId = m_level->m_levelID.value();
        if (levelId <= 0) return; // nivel local / test din editor

        // netezim miscarea celorlalti jucatori
        float t = std::min(1.f, dt * 12.f);
        for (auto& kv : m_fields->ghosts) {
            auto node = kv.second.node;
            if (!node) continue;
            auto cur = node->getPosition();
            node->setPosition(ccp(
                cur.x + (kv.second.target.x - cur.x) * t,
                cur.y + (kv.second.target.y - cur.y) * t
            ));
        }

        m_fields->sendT += dt;
        if (m_fields->sendT >= 0.25f) {
            m_fields->sendT = 0.f;
            mpSendPresence(
                levelId, std::string(m_level->m_levelName),
                m_player1->getPositionX(), m_player1->getPositionY(), m_player1->getRotation()
            );
        }

        m_fields->pollT += dt;
        if (m_fields->pollT >= 0.3f) {
            m_fields->pollT = 0.f;
            this->pollOthers(levelId);
        }
    }

    void pollOthers(int levelId) {
        if (g_mp.userId.empty()) return;
        auto req = mpRequest(true);
        s_pollTask.spawn(
            req.get(fmt::format(
                "{}/rest/v1/mp_online?select=user_id,username,x,y,rot,icon_id&level_id=eq.{}&user_id=neq.{}",
                MP_URL, levelId, g_mp.userId
            )),
            [self = Ref(this)](web::WebResponse res) {
                if (!self->getParent() || !res.ok()) return;
                auto jr = res.json();
                if (!jr) return;
                auto j = jr.unwrap();
                auto arr = j.asArray();
                if (!arr) return;
                self->applyGhosts(arr.unwrap());
            }
        );
    }

    void applyGhosts(std::vector<matjson::Value> items) {
        if (!g_mp.connected || !m_objectLayer) return;

        std::unordered_map<std::string, bool> seen;
        for (auto& it : items) {
            std::string id = jstr(it, "user_id");
            if (id.empty()) continue;
            seen[id] = true;

            float x = static_cast<float>(it["x"].asDouble().unwrapOr(0.0));
            float y = static_cast<float>(it["y"].asDouble().unwrapOr(0.0));
            float rot = static_cast<float>(it["rot"].asDouble().unwrapOr(0.0));
            int iconId = static_cast<int>(it["icon_id"].asInt().unwrapOr(1));

            auto found = m_fields->ghosts.find(id);
            if (found == m_fields->ghosts.end()) {
                auto node = CCNode::create();
                auto icon = SimplePlayer::create(iconId);
                icon->setOpacity(150);
                icon->setTag(1);
                node->addChild(icon);

                auto label = CCLabelBMFont::create(jstr(it, "username").c_str(), "bigFont.fnt");
                label->setScale(.3f);
                label->setPosition({0.f, 28.f});
                node->addChild(label);

                node->setPosition(ccp(x, y));
                m_objectLayer->addChild(node, 1000);

                GhostData ghost;
                ghost.node = node;
                ghost.target = ccp(x, y);
                m_fields->ghosts[id] = ghost;
                found = m_fields->ghosts.find(id);
            }
            found->second.target = ccp(x, y);
            if (auto icon = found->second.node->getChildByTag(1)) icon->setRotation(rot);
        }

        // scoatem jucatorii care au plecat
        for (auto iter = m_fields->ghosts.begin(); iter != m_fields->ghosts.end();) {
            if (!seen.count(iter->first)) {
                if (iter->second.node) iter->second.node->removeFromParent();
                iter = m_fields->ghosts.erase(iter);
            } else {
                ++iter;
            }
        }
    }

    void onQuit() {
        this->clearGhosts();
        mpSendPresence(0, "", 0.f, 0.f, 0.f);
        PlayLayer::onQuit();
    }
};

// ---------------- Butoanele din meniul Creator ----------------
static CCNode* makeTile(float S, CCNode* icon, float iconScale, char const* line1, char const* line2) {
    auto node = CCNode::create();
    node->setContentSize({S, S});

    auto bg = CCScale9Sprite::create("GJ_button_01.png", {0.f, 0.f, 40.f, 40.f});
    bg->setContentSize({S, S});
    bg->setPosition({S / 2.f, S / 2.f});
    node->addChild(bg);

    if (icon) {
        icon->setScale(iconScale);
        icon->setPosition({S / 2.f, S * 0.70f});
        node->addChild(icon);
    }

    auto name = CCLabelBMFont::create(line1, "goldFont.fnt");
    name->limitLabelWidth(S * 0.9f, .5f, .1f);
    name->setPosition({S / 2.f, S * 0.36f});
    node->addChild(name);

    auto lvls = CCLabelBMFont::create(line2, "goldFont.fnt");
    lvls->limitLabelWidth(S * 0.9f, .5f, .1f);
    lvls->setPosition({S / 2.f, S * 0.17f});
    node->addChild(lvls);

    return node;
}

class $modify(AndreiCreatorLayer, CreatorLayer) {
    bool init() {
        if (!CreatorLayer::init()) return false;

        // asteptam un cadru ca grila sa fie asezata, apoi punem butoanele peste "The Map" si "Saved"
        queueInMainThread([self = Ref(this)] {
            self->placeTile("map-button", 5, 0);
            self->placeTile("saved-button", 1, 1);
        });

        checkForUpdate();
        return true;
    }

    void placeTile(char const* id, size_t index, int which) {
        auto winSize = CCDirector::sharedDirector()->getWinSize();

        CCNode* target = nullptr;
        if (auto grid = this->getChildByID("creator-buttons-menu")) {
            target = grid->getChildByID(id);
            if (!target && grid->getChildrenCount() > index) {
                target = static_cast<CCNode*>(grid->getChildren()->objectAtIndex(index));
            }
        }

        float S = 75.f;
        CCPoint center = ccp(S / 2.f + 6.f, winSize.height / 2.f - (which == 1 ? S + 6.f : 0.f));

        if (target && target->getParent()) {
            auto parent = target->getParent();
            auto bb = target->boundingBox();
            auto bl = this->convertToNodeSpace(parent->convertToWorldSpace(bb.origin));
            auto tr = this->convertToNodeSpace(parent->convertToWorldSpace(
                ccp(bb.origin.x + bb.size.width, bb.origin.y + bb.size.height)
            ));
            float size = tr.x - bl.x;
            if (size > 30.f && size < 250.f) {
                S = size;
                center = ccp((bl.x + tr.x) / 2.f, (bl.y + tr.y) / 2.f);
                target->setVisible(false);
            }
        }

        CCNode* tile = nullptr;
        if (which == 0) {
            auto star = CCSprite::createWithSpriteFrameName("GJ_starsIcon_001.png");
            float sc = star ? (S * 0.34f / star->getContentSize().height) : 1.f;
            tile = makeTile(S, star, sc, "Andrei123459", "Levels");
        } else {
            auto gm = GameManager::sharedState();
            auto cube = SimplePlayer::create(gm->getPlayerFrame());
            cube->setColors(gm->colorForIdx(gm->getPlayerColor()), gm->colorForIdx(gm->getPlayerColor2()));
            tile = makeTile(S, cube, S * 0.34f / 30.f, "Andrei123459", "Multiplayer");
        }

        auto menu = CCMenu::create();
        menu->setID(which == 0 ? "andrei-levels-menu"_spr : "andrei-multiplayer-menu"_spr);
        menu->setPosition(center);

        CCMenuItemSpriteExtra* btn = nullptr;
        if (which == 0) {
            btn = CCMenuItemSpriteExtra::create(tile, this, menu_selector(AndreiCreatorLayer::onAndreiLevels));
        } else {
            btn = CCMenuItemSpriteExtra::create(tile, this, menu_selector(AndreiCreatorLayer::onAndreiMultiplayer));
        }
        menu->addChild(btn);
        this->addChild(menu, 10);
    }

    void onAndreiMultiplayer(CCObject*) {
        openMultiplayer();
    }

    void onAndreiLevels(CCObject*) {
        auto userID = Mod::get()->getSettingValue<int64_t>("user-id");
        if (userID <= 0) {
            FLAlertLayer::create(
                "Setare lipsa",
                "Pune <cy>User ID</c>-ul in setarile modului.",
                "OK"
            )->show();
            return;
        }

        auto search = GJSearchObject::create(SearchType::UsersLevels, std::to_string(userID));
        auto scene = LevelBrowserLayer::scene(search);
        CCDirector::sharedDirector()->pushScene(CCTransitionFade::create(.5f, scene));
    }
};

// ---------------- Istoric cautari ----------------
static std::vector<std::string> getSearchHistory() {
    return Mod::get()->getSavedValue<std::vector<std::string>>("search-history", {});
}

static void addToSearchHistory(std::string text) {
    // taiem spatiile de la capete
    while (!text.empty() && text.front() == ' ') text.erase(text.begin());
    while (!text.empty() && text.back() == ' ') text.pop_back();
    if (text.empty()) return;

    auto hist = getSearchHistory();
    for (size_t i = 0; i < hist.size(); i++) {
        if (hist[i] == text) {
            hist.erase(hist.begin() + i);
            break;
        }
    }
    hist.insert(hist.begin(), text);
    if (hist.size() > 20) hist.resize(20);
    Mod::get()->setSavedValue("search-history", hist);
}

class SearchHistoryPopup : public Popup {
protected:
    LevelSearchLayer* m_searchLayer = nullptr;
    std::vector<std::string> m_entries;

    bool init(LevelSearchLayer* layer) {
        if (!Popup::init(320.f, 270.f, "GJ_square01.png")) return false;
        m_searchLayer = layer;
        this->setTitle("Search History");

        m_entries = getSearchHistory();

        if (m_entries.empty()) {
            auto empty = CCLabelBMFont::create("Nu ai cautat nimic inca", "bigFont.fnt");
            empty->setScale(.45f);
            m_mainLayer->addChildAtPosition(empty, Anchor::Center, ccp(0, 0));
            return true;
        }

        size_t count = std::min<size_t>(m_entries.size(), 6);
        for (size_t i = 0; i < count; i++) {
            std::string label = m_entries[i];
            if (label.size() > 28) label = label.substr(0, 25) + "...";

            auto spr = ButtonSprite::create(
                label.c_str(), 260, true, "bigFont.fnt", "GJ_button_01.png", 28.f, .55f
            );
            auto btn = CCMenuItemSpriteExtra::create(
                spr, this, menu_selector(SearchHistoryPopup::onPick)
            );
            btn->setTag(static_cast<int>(i));
            m_buttonMenu->addChildAtPosition(btn, Anchor::Top, ccp(0, -56.f - i * 32.f));
        }

        auto clearSpr = ButtonSprite::create("Sterge istoric", 140, true, "goldFont.fnt", "GJ_button_01.png", 26.f, .6f);
        auto clearBtn = CCMenuItemSpriteExtra::create(
            clearSpr, this, menu_selector(SearchHistoryPopup::onClear)
        );
        m_buttonMenu->addChildAtPosition(clearBtn, Anchor::Bottom, ccp(0, 24));

        return true;
    }

    void onPick(CCObject* sender) {
        int idx = static_cast<CCNode*>(sender)->getTag();
        if (idx >= 0 && idx < static_cast<int>(m_entries.size()) && m_searchLayer) {
            m_searchLayer->m_searchInput->setString(m_entries[idx]);
        }
        this->onClose(nullptr);
    }

    void onClear(CCObject*) {
        Mod::get()->setSavedValue("search-history", std::vector<std::string>{});
        this->onClose(nullptr);
    }

public:
    static SearchHistoryPopup* create(LevelSearchLayer* layer) {
        auto ret = new SearchHistoryPopup();
        if (ret->init(layer)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

class $modify(AndreiSearchLayer, LevelSearchLayer) {
    bool init(int type) {
        if (!LevelSearchLayer::init(type)) return false;

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        menu->setID("andrei-history-menu"_spr);

        auto spr = ButtonSprite::create("Istoric", 70, true, "goldFont.fnt", "GJ_button_01.png", 30.f, .7f);
        auto btn = CCMenuItemSpriteExtra::create(
            spr, this, menu_selector(AndreiSearchLayer::onAndreiHistory)
        );
        btn->setID("andrei-history-button"_spr);
        btn->setPosition({45.f, winSize.height - 100.f});
        menu->addChild(btn);

        this->addChild(menu, 10);
        return true;
    }

    void onAndreiHistory(CCObject*) {
        if (auto popup = SearchHistoryPopup::create(this)) popup->show();
    }

    void onSearch(CCObject* sender) {
        addToSearchHistory(std::string(m_searchInput->getString()));
        LevelSearchLayer::onSearch(sender);
    }

    void onSearchUser(CCObject* sender) {
        addToSearchHistory(std::string(m_searchInput->getString()));
        LevelSearchLayer::onSearchUser(sender);
    }
};
