#include <Geode/Geode.hpp>
#include <Geode/modify/CustomSongLayer.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/LevelSettingsLayer.hpp>

#include <cocos/network/HttpClient.h>
#include <fstream>
#include <unordered_map>

using namespace geode::prelude;
using namespace cocos2d::network;

class SongURLDownloader : public CCNode {
protected:
    std::unordered_map<CCHttpRequest*, int> m_pendingSongIds;

public:
    static SongURLDownloader* get() {
        static SongURLDownloader* instance = nullptr;
        if (!instance) {
            instance = new SongURLDownloader();
            instance->init();
            instance->retain();
        }
        return instance;
    }

    void downloadSongFromURL(std::string const& url, int songID) {
        auto request = new CCHttpRequest();
        request->setUrl(url.c_str());
        request->setRequestType(CCHttpRequest::kHttpGet);
        request->setTag("more-songs-downloads");
        request->setResponseCallback(this, httpresponse_selector(SongURLDownloader::onSongDownloaded));

        m_pendingSongIds.emplace(request, songID);
        CCHttpClient::getInstance()->send(request);
        request->release();
    }

    void onSongDownloaded(CCHttpClient*, CCHttpResponse* response) {
        if (!response) {
            FLAlertLayer::create("Music URL", "Download failed (no response).", "OK")->show();
            return;
        }

        auto request = response->getHttpRequest();
        if (!request || !m_pendingSongIds.contains(request)) {
            FLAlertLayer::create("Music URL", "Unexpected download state.", "OK")->show();
            return;
        }

        auto songID = m_pendingSongIds.at(request);
        m_pendingSongIds.erase(request);

        if (!response->isSucceed()) {
            FLAlertLayer::create("Music URL", response->getErrorBuffer(), "OK")->show();
            return;
        }

        auto path = MusicDownloadManager::sharedState()->pathForSong(songID);
        auto data = response->getResponseData();

        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) {
            FLAlertLayer::create("Music URL", "Couldn't write downloaded song file.", "OK")->show();
            return;
        }

        file.write(data->data(), static_cast<std::streamsize>(data->size()));
        file.close();

        FLAlertLayer::create(
            "Music URL",
            fmt::format(
                "Downloaded and installed successfully.\\nUse Song ID: <cg>{}</c>",
                songID
            ).c_str(),
            "OK"
        )->show();
    }
};

class SongURLImportPopup : public Popup<> {
protected:
    TextInput* m_urlInput = nullptr;

    bool setup() override {
        this->setTitle("Insert music URL:");

        m_urlInput = TextInput::create(250.f, "https://...");
        m_urlInput->setPosition(m_size.width / 2.f, m_size.height / 2.f + 5.f);
        m_mainLayer->addChild(m_urlInput);

        auto menu = CCMenu::create();
        menu->setPosition({ m_size.width / 2.f, 35.f });
        m_mainLayer->addChild(menu);

        auto confirmSpr = ButtonSprite::create("Confirm");
        auto confirmBtn = CCMenuItemSpriteExtra::create(
            confirmSpr,
            this,
            menu_selector(SongURLImportPopup::onConfirm)
        );
        menu->addChild(confirmBtn);

        return true;
    }

    int songIDForURL(std::string const& url) {
        auto hashValue = static_cast<uint32_t>(std::hash<std::string>{}(url));
        return static_cast<int>(10000000 + (hashValue % 89999999));
    }

    void onConfirm(CCObject*) {
        auto url = m_urlInput->getString();
        if (url.empty()) {
            FLAlertLayer::create("Music URL", "Please paste a URL first.", "OK")->show();
            return;
        }

        if (!string::startsWith(url, "http://") && !string::startsWith(url, "https://")) {
            FLAlertLayer::create("Music URL", "The URL must start with http:// or https://", "OK")->show();
            return;
        }

        auto songID = songIDForURL(url);
        SongURLDownloader::get()->downloadSongFromURL(url, songID);

        FLAlertLayer::create(
            "Music URL",
            fmt::format("Downloading now...\\nGenerated Song ID: <cy>{}</c>", songID).c_str(),
            "OK"
        )->show();
        this->onClose(nullptr);
    }

public:
    static SongURLImportPopup* create() {
        auto ret = new SongURLImportPopup();
        if (ret && ret->initAnchored(300.f, 190.f)) {
            ret->autorelease();
            return ret;
        }

        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};


class PopupOpenTarget : public CCNode {
public:
    static PopupOpenTarget* get() {
        static PopupOpenTarget* inst = nullptr;
        if (!inst) {
            inst = new PopupOpenTarget();
            inst->init();
            inst->retain();
        }
        return inst;
    }

    void onOpen(CCObject*) {
        if (auto popup = SongURLImportPopup::create()) {
            popup->show();
        }
    }
};

static void addOpenURLButton(CCLayer* layer, CCPoint position, std::string const& id) {
    auto menu = typeinfo_cast<CCMenu*>(layer->getChildByID("url-import-menu"));
    if (!menu) {
        menu = CCMenu::create();
        menu->setID("url-import-menu");
        menu->setPosition({0.f, 0.f});
        layer->addChild(menu, 50);
    }

    if (menu->getChildByID(id)) {
        return;
    }

    auto button = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("URL", "goldFont.fnt", "GJ_button_04.png", 0.8f),
        PopupOpenTarget::get(),
        menu_selector(PopupOpenTarget::onOpen)
    );

    button->setID(id);
    button->setPosition(position);
    menu->addChild(button);
}

class $modify(LevelSettingsLayerMusicURL, LevelSettingsLayer) {
    bool init(LevelSettingsObject* settings, LevelEditorLayer* editor) {
        if (!LevelSettingsLayer::init(settings, editor)) {
            return false;
        }

        addOpenURLButton(this, { 40.f, 255.f }, "url-import-btn-editor"_spr);
        return true;
    }
};

class $modify(EditLevelLayerMusicURL, EditLevelLayer) {
    bool init(GJGameLevel* level) {
        if (!EditLevelLayer::init(level)) {
            return false;
        }

        addOpenURLButton(this, { 40.f, 255.f }, "url-import-btn-outside-editor"_spr);
        return true;
    }
};

class $modify(LevelInfoLayerMusicURL, LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) {
            return false;
        }

        addOpenURLButton(this, { 40.f, 255.f }, "url-import-btn-level-info"_spr);
        return true;
    }
};

class $modify(CustomSongLayerMusicURL, CustomSongLayer) {
    bool init(LevelSettingsObject* settings) {
        if (!CustomSongLayer::init(settings)) {
            return false;
        }

        addOpenURLButton(this, { 35.f, 250.f }, "url-import-btn-custom-song"_spr);
        return true;
    }
};
