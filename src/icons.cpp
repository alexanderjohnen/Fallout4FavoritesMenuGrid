#include "PCH.h"

#include "icons.h"

namespace
{
	// The first attempt waited on bytesLoaded against bytesTotal and got
	// "0 bytes of 0" -- which says the load never started but not why. So the
	// loader is asked properly now: Movie::CreateFunction turns a C++ object
	// into something ActionScript can call, and the events carry the reason
	// in words.
	enum class Outcome
	{
		kWaiting,
		kArrived,
		kFailed
	};

	std::atomic<Outcome> g_outcome{ Outcome::kWaiting };
	std::mutex g_wordsLock;
	std::string g_words;

	RE::Scaleform::GFx::Value g_loader;
	RE::Scaleform::GFx::Value g_request;
	RE::Scaleform::GFx::Value g_context;
	RE::Scaleform::GFx::Value g_listener;

	// Where to look, in order. A name with no slash in it is tried in every
	// shape a Scaleform URL is known to take, because which one is right
	// depends on what the player's file opener makes of a relative path --
	// and that is the game's, not ours.
	std::vector<std::string> g_candidates;
	std::size_t g_candidate = 0;
	bool g_givenUp = false;

	[[nodiscard]] std::string TextOf(const RE::Scaleform::GFx::Value& a_object)
	{
		RE::Scaleform::GFx::Value value;
		if (a_object.IsObject() && a_object.GetMember("text", &value) &&
			value.IsString()) {
			return value.GetString();
		}
		return {};
	}

	[[nodiscard]] std::string StringMember(
		const RE::Scaleform::GFx::Value& a_object,
		const char* a_member)
	{
		RE::Scaleform::GFx::Value value;
		if (a_object.IsObject() && a_object.GetMember(a_member, &value) &&
			value.IsString()) {
			return value.GetString();
		}
		return {};
	}

	// One handler for every event we listen for. It only writes down what
	// happened; acting on it waits for the next frame, where the menu is
	// known to still be there.
	class Listener : public RE::Scaleform::GFx::FunctionHandler
	{
	public:
		void Call(const Params& a_params) override
		{
			if (a_params.argCount < 1) {
				return;
			}
			const auto& event = a_params.args[0];
			const auto type = StringMember(event, "type");
			const auto text = TextOf(event);

			{
				const std::scoped_lock guard{ g_wordsLock };
				g_words = text.empty() ? type : std::format("{}: {}", type, text);
			}
			g_outcome = type == "complete" ? Outcome::kArrived : Outcome::kFailed;
		}
	};

	// A static, because CreateFunction takes a reference to it and the movie
	// keeps that reference for as long as the listener is attached.
	Listener g_handler;

	[[nodiscard]] bool CurrentDomain(
		RE::IMenu* a_canvas,
		RE::Scaleform::GFx::Value& a_domain)
	{
		RE::Scaleform::GFx::Value root;
		RE::Scaleform::GFx::Value info;
		if (!a_canvas->uiMovie->GetVariable(&root, "root") || !root.IsObject() ||
			!root.GetMember("loaderInfo", &info) || !info.IsObject()) {
			return false;
		}

		// What the movie itself was loaded from. It says exactly what shape a
		// path takes in this player, which is the one thing guessing cannot
		// settle.
		static bool said = false;
		if (!said) {
			said = true;
			logger::info("icons: this movie came from \"{}\"", StringMember(info, "url"));
		}

		return info.GetMember("applicationDomain", &a_domain) && a_domain.IsObject();
	}

	[[nodiscard]] std::vector<std::string> ShapesOf(const std::string& a_library)
	{
		if (a_library.find('/') != std::string::npos ||
			a_library.find('\\') != std::string::npos) {
			return { a_library };
		}
		return { a_library,
			"Interface/" + a_library,
			"../" + a_library,
			"Data/Interface/" + a_library };
	}

	void Attach(RE::IMenu* a_canvas, RE::Scaleform::GFx::Value& a_info)
	{
		a_canvas->uiMovie->CreateFunction(&g_listener, &g_handler);
		if (!g_listener.IsObject()) {
			logger::warn("icons: this movie will not take a function of ours");
			return;
		}
		for (const auto* event :
			{ "complete", "ioError", "securityError" }) {
			const std::array listen{ RE::Scaleform::GFx::Value(event), g_listener };
			a_info.Invoke(
				"addEventListener",
				nullptr,
				listen.data(),
				static_cast<std::uint32_t>(listen.size()));
		}
	}

	// One try at one shape of the path.
	bool Ask(RE::IMenu* a_canvas, const std::string& a_url)
	{
		RE::Scaleform::GFx::Value domain;
		if (!CurrentDomain(a_canvas, domain)) {
			logger::warn("icons: this movie will not say which domain it is in");
			return false;
		}

		// The domain is the whole point of the second argument: the default
		// would make a child domain, and a child's classes are not the ones
		// CreateObject finds.
		const std::array context{ RE::Scaleform::GFx::Value(false), domain };
		a_canvas->uiMovie->CreateObject(
			&g_context,
			"flash.system.LoaderContext",
			context.data(),
			static_cast<std::uint32_t>(context.size()));

		const RE::Scaleform::GFx::Value url{ a_url.c_str() };
		a_canvas->uiMovie->CreateObject(&g_request, "flash.net.URLRequest", &url, 1);
		a_canvas->uiMovie->CreateObject(&g_loader, "flash.display.Loader");
		if (!g_loader.IsObject() || !g_request.IsObject() || !g_context.IsObject()) {
			logger::warn("icons: the loader would not be built");
			return false;
		}

		RE::Scaleform::GFx::Value info;
		if (g_loader.GetMember("contentLoaderInfo", &info) && info.IsObject()) {
			Attach(a_canvas, info);
		}

		g_outcome = Outcome::kWaiting;
		const std::array load{ g_request, g_context };
		const auto asked = g_loader.Invoke(
			"load", nullptr, load.data(), static_cast<std::uint32_t>(load.size()));
		logger::info(
			"icons: asked for \"{}\" -- the call itself {}",
			a_url,
			asked ? "went through" : "was refused");
		return true;
	}
}

bool icons::Begin(RE::IMenu* a_canvas, const std::string& a_library)
{
	if (a_library.empty() || !a_canvas || !a_canvas->uiMovie) {
		return false;
	}

	Release();
	g_candidates = ShapesOf(a_library);
	g_candidate = 0;
	g_givenUp = false;
	return Ask(a_canvas, g_candidates.front());
}

void icons::Poll(RE::IMenu* a_canvas, void (*a_ready)())
{
	if (g_givenUp || !a_canvas || !g_loader.IsObject()) {
		return;
	}

	const auto outcome = g_outcome.load();
	if (outcome == Outcome::kWaiting) {
		return;
	}

	std::string words;
	{
		const std::scoped_lock guard{ g_wordsLock };
		words = g_words;
	}

	if (outcome == Outcome::kArrived) {
		g_givenUp = false;
		logger::info(
			"icons: \"{}\" is in ({})", g_candidates[g_candidate], words);
		g_outcome = Outcome::kWaiting;
		if (a_ready) {
			a_ready();
		}
		return;
	}

	logger::warn("icons: \"{}\" -- {}", g_candidates[g_candidate], words);
	if (++g_candidate >= g_candidates.size()) {
		g_givenUp = true;
		logger::warn("icons: no shape of that name could be loaded");
		return;
	}
	Ask(a_canvas, g_candidates[g_candidate]);
}

bool icons::Ready()
{
	return g_outcome == Outcome::kArrived;
}

void icons::Release()
{
	if (g_loader.IsObject()) {
		g_loader.Invoke("unload");
	}
	g_loader = RE::Scaleform::GFx::Value();
	g_request = RE::Scaleform::GFx::Value();
	g_context = RE::Scaleform::GFx::Value();
	g_listener = RE::Scaleform::GFx::Value();
	g_outcome = Outcome::kWaiting;
	g_givenUp = false;
	g_candidate = 0;
}
