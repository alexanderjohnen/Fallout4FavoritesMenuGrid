#include "PCH.h"

#include "icons.h"

#include <deque>

namespace
{
	// The first attempt waited on bytesLoaded against bytesTotal and got
	// "0 bytes of 0" -- which says the load never started and nothing about
	// why. So the loader is asked properly: Movie::CreateFunction turns a C++
	// object into something ActionScript can call, and the events carry the
	// reason in words.
	enum class Outcome
	{
		kWaiting,
		kArrived,
		kFailed
	};

	std::atomic<Outcome> g_outcome{ Outcome::kWaiting };
	std::mutex g_wordsLock;
	std::string g_words;

	// One at a time. Several loaders at once would be faster and would also
	// mean several answers arriving in one frame with no way to tell whose is
	// whose, and there is nothing here worth that.
	struct Job
	{
		std::string library;
		std::vector<std::string> shapes;
		std::size_t shape{ 0 };
	};

	std::deque<Job> g_queue;
	std::optional<Job> g_active;
	std::set<std::string> g_asked;
	std::set<std::string> g_have;

	RE::Scaleform::GFx::Value g_loader;
	RE::Scaleform::GFx::Value g_request;
	RE::Scaleform::GFx::Value g_context;
	RE::Scaleform::GFx::Value g_listener;

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
			const auto text = StringMember(event, "text");

			{
				const std::scoped_lock guard{ g_wordsLock };
				g_words = text.empty() ? type : std::format("{}: {}", type, text);
			}
			g_outcome = type == "complete" ? Outcome::kArrived : Outcome::kFailed;
		}
	};

	// Static, because CreateFunction keeps a reference for as long as the
	// listener is attached, and one object per menu opening would be a leak
	// with notice.
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
			logger::info(
				"icons: this movie came from \"{}\"", StringMember(info, "url"));
		}

		return info.GetMember("applicationDomain", &a_domain) && a_domain.IsObject();
	}

	// Where to look, in order. A name with no separator in it is tried in
	// every shape a Scaleform URL is known to take, because which one is
	// right depends on what the player's file opener makes of a relative
	// path -- and that is the game's business, not ours. A name that already
	// carries a path is taken as given.
	[[nodiscard]] std::vector<std::string> ShapesOf(const std::string& a_library)
	{
		if (a_library.find('/') != std::string::npos ||
			a_library.find('\\') != std::string::npos) {
			return { a_library, "Interface/" + a_library };
		}
		return { a_library, "Interface/" + a_library, "../" + a_library };
	}

	void Attach(RE::IMenu* a_canvas, RE::Scaleform::GFx::Value& a_info)
	{
		a_canvas->uiMovie->CreateFunction(&g_listener, &g_handler);
		if (!g_listener.IsObject()) {
			logger::warn("icons: this movie will not take a function of ours");
			return;
		}
		for (const auto* event : { "complete", "ioError", "securityError" }) {
			const std::array listen{ RE::Scaleform::GFx::Value(event), g_listener };
			a_info.Invoke(
				"addEventListener",
				nullptr,
				listen.data(),
				static_cast<std::uint32_t>(listen.size()));
		}
	}

	// One try at one shape of one path.
	bool Ask(RE::IMenu* a_canvas, const std::string& a_url)
	{
		RE::Scaleform::GFx::Value domain;
		if (!CurrentDomain(a_canvas, domain)) {
			logger::warn("icons: this movie will not say which domain it is in");
			return false;
		}

		// The domain is the whole point of the second argument: the default
		// would make a child domain, and a child's classes are not the ones
		// CreateObject finds -- which from outside looks exactly like a load
		// that failed.
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
		g_loader.Invoke(
			"load", nullptr, load.data(), static_cast<std::uint32_t>(load.size()));
		return true;
	}

	void Drop()
	{
		g_loader = RE::Scaleform::GFx::Value();
		g_request = RE::Scaleform::GFx::Value();
		g_context = RE::Scaleform::GFx::Value();
		g_listener = RE::Scaleform::GFx::Value();
		g_outcome = Outcome::kWaiting;
	}

	// Starts the next library, if there is one and nothing is in flight.
	void Next(RE::IMenu* a_canvas)
	{
		if (g_active || g_queue.empty()) {
			return;
		}
		g_active = g_queue.front();
		g_queue.pop_front();
		if (!Ask(a_canvas, g_active->shapes[g_active->shape])) {
			g_active.reset();
		}
	}
}

void icons::Want(RE::IMenu* a_canvas, const std::string& a_library)
{
	if (a_library.empty() || !a_canvas || !a_canvas->uiMovie ||
		!g_asked.insert(a_library).second) {
		return;
	}
	g_queue.push_back(Job{ a_library, ShapesOf(a_library), 0 });
	Next(a_canvas);
}

void icons::Poll(RE::IMenu* a_canvas, void (*a_changed)())
{
	if (!a_canvas || !a_canvas->uiMovie) {
		return;
	}
	if (!g_active) {
		Next(a_canvas);
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
		logger::info("icons: \"{}\" is in", g_active->shapes[g_active->shape]);
		g_have.insert(g_active->library);
		g_active.reset();
		Drop();
		Next(a_canvas);
		if (a_changed) {
			a_changed();
		}
		return;
	}

	// The next shape of the same name, and after the last one, the next
	// library. A missing addon library is ordinary -- its mod is simply not
	// installed -- so this is one line, not a warning storm.
	Drop();
	if (++g_active->shape < g_active->shapes.size()) {
		if (!Ask(a_canvas, g_active->shapes[g_active->shape])) {
			g_active.reset();
		}
		return;
	}

	logger::info("icons: no \"{}\" anywhere -- {}", g_active->library, words);
	g_active.reset();
	Next(a_canvas);
}

bool icons::Has(const std::string& a_library)
{
	return g_have.contains(a_library);
}

void icons::Release()
{
	if (g_loader.IsObject()) {
		g_loader.Invoke("unload");
	}
	Drop();
	g_queue.clear();
	g_active.reset();
	// The classes went with the movie, so nothing is remembered as loaded.
	g_asked.clear();
	g_have.clear();
}
