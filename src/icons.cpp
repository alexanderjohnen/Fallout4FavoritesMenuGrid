#include "PCH.h"

#include "icons.h"

namespace
{
	// A library that has not arrived in this many frames is not going to. A
	// missing file leaves bytesTotal at zero for ever, and a plugin that
	// asked about it once a frame until the game closed would be its own
	// kind of bug.
	constexpr int kPatience = 600;

	RE::Scaleform::GFx::Value g_loader;
	RE::Scaleform::GFx::Value g_request;
	RE::Scaleform::GFx::Value g_context;
	bool g_ready = false;
	bool g_givenUp = false;
	int g_frames = 0;

	[[nodiscard]] double ReadNumber(
		const RE::Scaleform::GFx::Value& a_object,
		const char* a_member)
	{
		RE::Scaleform::GFx::Value value;
		if (!a_object.IsObject() || !a_object.GetMember(a_member, &value)) {
			return 0.0;
		}
		if (value.IsNumber()) {
			return value.GetNumber();
		}
		if (value.IsInt()) {
			return static_cast<double>(value.GetInt());
		}
		if (value.IsUInt()) {
			return static_cast<double>(value.GetUInt());
		}
		return 0.0;
	}

	// The domain this movie lives in. It cannot be built, only found: a
	// library loaded into a domain of its own would register its classes
	// where CreateObject never looks, which would look exactly like a load
	// that failed.
	[[nodiscard]] bool CurrentDomain(
		RE::IMenu* a_canvas,
		RE::Scaleform::GFx::Value& a_domain)
	{
		RE::Scaleform::GFx::Value root;
		RE::Scaleform::GFx::Value info;
		return a_canvas->uiMovie->GetVariable(&root, "root") && root.IsObject() &&
			root.GetMember("loaderInfo", &info) && info.IsObject() &&
			info.GetMember("applicationDomain", &a_domain) && a_domain.IsObject();
	}
}

bool icons::Begin(RE::IMenu* a_canvas, const std::string& a_library)
{
	if (a_library.empty() || !a_canvas || !a_canvas->uiMovie ||
		g_loader.IsObject()) {
		return false;
	}

	g_ready = false;
	g_givenUp = false;
	g_frames = 0;

	RE::Scaleform::GFx::Value domain;
	if (!CurrentDomain(a_canvas, domain)) {
		logger::warn("icons: this movie will not say which domain it is in");
		return false;
	}

	// Two arguments: no policy file, and the domain we were just handed. The
	// second is the whole point -- the default would make a child domain, and
	// a child's classes are not the ones CreateObject finds.
	const std::array context{ RE::Scaleform::GFx::Value(false), domain };
	a_canvas->uiMovie->CreateObject(
		&g_context,
		"flash.system.LoaderContext",
		context.data(),
		static_cast<std::uint32_t>(context.size()));

	const RE::Scaleform::GFx::Value url{ a_library.c_str() };
	a_canvas->uiMovie->CreateObject(&g_request, "flash.net.URLRequest", &url, 1);

	a_canvas->uiMovie->CreateObject(&g_loader, "flash.display.Loader");
	if (!g_loader.IsObject() || !g_request.IsObject() || !g_context.IsObject()) {
		logger::warn("icons: the loader would not be built");
		Release();
		return false;
	}

	const std::array load{ g_request, g_context };
	g_loader.Invoke(
		"load", nullptr, load.data(), static_cast<std::uint32_t>(load.size()));
	logger::info("icons: asked for {}", a_library);
	return true;
}

void icons::Poll(RE::IMenu* a_canvas, void (*a_ready)())
{
	if (g_ready || g_givenUp || !g_loader.IsObject() || !a_canvas) {
		return;
	}

	RE::Scaleform::GFx::Value info;
	if (!g_loader.GetMember("contentLoaderInfo", &info) || !info.IsObject()) {
		g_givenUp = true;
		logger::warn("icons: the loader has nothing to report");
		return;
	}

	const auto total = ReadNumber(info, "bytesTotal");
	const auto loaded = ReadNumber(info, "bytesLoaded");
	if (total <= 0.0 || loaded < total) {
		if (++g_frames > kPatience) {
			g_givenUp = true;
			logger::warn(
				"icons: nothing arrived in {} frames -- {} bytes of {}",
				kPatience,
				loaded,
				total);
		}
		return;
	}

	g_ready = true;
	logger::info("icons: {:.0f} bytes are in", total);
	if (a_ready) {
		a_ready();
	}
}

bool icons::Ready()
{
	return g_ready;
}

void icons::Release()
{
	if (g_loader.IsObject()) {
		g_loader.Invoke("unload");
	}
	g_loader = RE::Scaleform::GFx::Value();
	g_request = RE::Scaleform::GFx::Value();
	g_context = RE::Scaleform::GFx::Value();
	g_ready = false;
	g_givenUp = false;
	g_frames = 0;
}
