#include <Geode/Geode.hpp>
#include <Geode/modify/CreatorLayer.hpp>
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

// ---------------- Butonul din meniul Creator ----------------
static CCNode* makeAndreiTile(float S) {
    auto node = CCNode::create();
    node->setContentSize({S, S});

    auto bg = CCSprite::createWithSpriteFrameName("GJ_button_01.png");
    auto bs = bg->getContentSize();
    bg->setScaleX(S / bs.width);
    bg->setScaleY(S / bs.height);
    bg->setPosition({S / 2.f, S / 2.f});
    node->addChild(bg);

    auto star = CCSprite::createWithSpriteFrameName("GJ_starsIcon_001.png");
    if (star) {
        star->setScale(S * 0.34f / star->getContentSize().height);
        star->setPosition({S / 2.f, S * 0.70f});
        node->addChild(star);
    }

    auto name = CCLabelBMFont::create("Andrei123459", "goldFont.fnt");
    name->limitLabelWidth(S * 0.9f, .5f, .1f);
    name->setPosition({S / 2.f, S * 0.36f});
    node->addChild(name);

    auto lvls = CCLabelBMFont::create("Levels", "goldFont.fnt");
    lvls->limitLabelWidth(S * 0.9f, .5f, .1f);
    lvls->setPosition({S / 2.f, S * 0.17f});
    node->addChild(lvls);

    return node;
}

class $modify(AndreiCreatorLayer, CreatorLayer) {
    bool init() {
        if (!CreatorLayer::init()) return false;

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        const float S = 75.f;

        auto menu = CCMenu::create();
        menu->setID("andrei-levels-menu"_spr);
        menu->setPosition({S / 2.f + 6.f, winSize.height / 2.f});

        auto btn = CCMenuItemSpriteExtra::create(
            makeAndreiTile(S), this, menu_selector(AndreiCreatorLayer::onAndreiLevels)
        );
        btn->setID("andrei-levels-button"_spr);
        menu->addChild(btn);

        this->addChild(menu, 10);

        checkForUpdate();
        return true;
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
