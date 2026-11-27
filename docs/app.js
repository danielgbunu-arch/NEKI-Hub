/* NEKI Hub: sitio informativo + demo LOCAL + monitoreo de datos FÍSICOS.
   Nunca se envían las acciones de la demo visual al panel de telemetría. */
"use strict";
(() => {
  const cfg = window.NEKI_CONFIG || {};
  const $ = (id) => document.getElementById(id);
  const safeLink = (url) => {
    try {
      const u = new URL(url);
      return ["https:", "http:"].includes(u.protocol) ? u.href : "";
    } catch {
      return "";
    }
  };
  const safeAsset = (url) =>
    typeof url === "string" &&
    url.trim() &&
    !/^\s*(javascript:|data:)/i.test(url)
      ? url.trim()
      : "";
  const text = (el, value) => {
    if (el) el.textContent = String(value);
  };
  const make = (tag, cls, txt) => {
    const el = document.createElement(tag);
    if (cls) el.className = cls;
    if (txt !== undefined) el.textContent = txt;
    return el;
  };

  /* Navegación adaptable */
  const sidebar = $("sidebar");
  const shade = $("mobileShade");
  const menu = $("menuToggle");
  function closeMenu() {
    sidebar.classList.remove("is-open");
    shade.classList.remove("is-open");
    menu.setAttribute("aria-expanded", "false");
  }
  menu.addEventListener("click", () => {
    const show = !sidebar.classList.contains("is-open");
    sidebar.classList.toggle("is-open", show);
    shade.classList.toggle("is-open", show);
    menu.setAttribute("aria-expanded", String(show));
  });
  shade.addEventListener("click", closeMenu);
  document.addEventListener("keydown", (e) => {
    if (e.key === "Escape") closeMenu();
  });
  const links = [...document.querySelectorAll(".nav-link")];
  for (const a of links) a.addEventListener("click", closeMenu);
  const sectionNames = Object.fromEntries(
    links.map((a) => [a.hash.slice(1), a.querySelector("span").textContent]),
  );
  const observer = new IntersectionObserver(
    (entries) => {
      const view = entries
        .filter((e) => e.isIntersecting)
        .sort((a, b) => b.intersectionRatio - a.intersectionRatio)[0];
      if (!view) return;
      const id = view.target.id;
      for (const a of links) {
        const active = a.hash === "#" + id;
        a.classList.toggle("active", active);
        if (active) a.setAttribute("aria-current", "location");
        else a.removeAttribute("aria-current");
      }
      text($("currentSection"), sectionNames[id] || "Inicio");
    },
    { rootMargin: "-15% 0px -60% 0px", threshold: 0 },
  );
  document
    .querySelectorAll(".page-section")
    .forEach((s) => observer.observe(s));
  text($("footerYear"), new Date().getFullYear());

  /* Información editable centralizada en config.js */
  if (cfg.company)
    document.querySelector(".contact-brand h3").textContent = cfg.company;
  if (cfg.tagline)
    document.querySelector(".contact-brand p").textContent = cfg.tagline;
  if (cfg.contactEmail) {
    text($("contactEmail"), cfg.contactEmail);
    text(
      $("mailCaution"),
      cfg.contactEmail.endsWith(".example")
        ? "Dirección de ejemplo; aún no habilitada."
        : "Contacto del proyecto",
    );
  }
  const protoPhoto = safeAsset(cfg.media?.prototypePhoto);
  if (protoPhoto) {
    $("prototypePhoto").src = protoPhoto;
    $("prototypePhoto").hidden = false;
    $("prototypePhoto").addEventListener("error", () => {
      $("prototypePhoto").hidden = true;
    });
  }
  const gallery = $("galleryMount");
  const defaults = [
    "Componentes utilizados",
    "Montaje del circuito",
    "Pruebas del prototipo",
    "Maqueta terminada",
  ];
  const galleryList = Array.isArray(cfg.media?.gallery)
    ? cfg.media.gallery
    : [];

  defaults.forEach((d, i) => {
    const item = galleryList[i] || {};
    const card = make("article", "gallery-card");
    const photo = make("div", "gallery-photo");

    const imgSrc = safeAsset(item.src);
    const videoSrc = safeAsset(item.video);

    const empty = () => {
      photo.replaceChildren();
      const p = make("div", "photo-placeholder");
      p.append(make("span", "", "▧"), make("small", "", "Foto real pendiente"));
      photo.append(p);
    };

    if (videoSrc) {
      const video = make("video", "gallery-video");

      video.src = videoSrc;
      video.controls = true;
      video.preload = "metadata";
      video.playsInline = true;

      photo.append(video);
    } else if (imgSrc) {
      const img = make("img");

      img.alt = "Evidencia fotográfica real: " + (item.title || d);
      img.loading = "lazy";
      img.addEventListener("error", empty);
      img.src = imgSrc;

      photo.append(img);
    } else {
      empty();
    }

    const caption = make("div", "gallery-caption");
    caption.append(
      make("strong", "", item.title || d),
      make("small", "", item.subtitle || "Fotografía real del proceso"),
    );

    card.append(photo, caption);
    gallery.append(card);
  });

  const team = $("teamMount");
  const teamList = Array.isArray(cfg.team) ? cfg.team : [];
  for (let i = 0; i < 8; i++) {
    const m = teamList[i] || {};
    const name = (m.name || "").trim();
    const card = make("article", "team-card");
    const photo = make("div", "team-photo");
    const imgSrc = safeAsset(m.photo);
    const empty = () => {
      photo.replaceChildren(
        make(
          "div",
          "team-initials",
          name
            ? name
                .split(/\s+/)
                .slice(0, 2)
                .map((w) => w[0])
                .join("")
                .toUpperCase()
            : String(i + 1).padStart(2, "0"),
        ),
      );
    };
    if (imgSrc) {
      const img = make("img");
      img.src = imgSrc;
      img.alt = "Fotografía de " + (name || "integrante " + (i + 1));
      img.loading = "lazy";
      img.addEventListener("error", empty);
      photo.append(img);
    } else empty();
    const info = make("div", "team-info");
    info.append(
      make("b", "", name || `Integrante ${i + 1} · nombre pendiente`),
      make(
        "small",
        "",
        (m.role || "").trim() || "Función pendiente de confirmar",
      ),
    );
    card.append(photo, info);
    team.append(card);
  }
  function actionLink(mountId, url, caption) {
    const target = $(mountId);
    const valid = safeLink(url);
    if (!valid) return;
    target.replaceChildren();
    const a = make("a", "button button-primary", caption + " ↗");
    a.href = valid;
    a.target = "_blank";
    a.rel = "noopener noreferrer";
    target.append(a);
  }
  actionLink(
    "simulationLinkMount",
    cfg.simulationUrl,
    "Abrir circuito electrónico",
  );
  actionLink("githubLinkMount", cfg.githubUrl, "Ver código en GitHub");
  if (Array.isArray(cfg.socialLinks) && cfg.socialLinks.length) {
    const container = $("socialMount");
    container.replaceChildren();
    let count = 0;
    cfg.socialLinks.forEach((v) => {
      const u = safeLink(v.url);
      if (!u) return;
      const a = make("a", "", v.label || "Red social");
      a.href = u;
      a.target = "_blank";
      a.rel = "noopener noreferrer";
      container.append(a);
      count++;
    });
    if (!count)
      container.append(
        make("span", "pending-tag", "Redes del equipo por agregar"),
      );
  }
  const videoFile = safeAsset(cfg.media?.demoVideo);
  if (videoFile) {
    const mount = $("videoMount");
    mount.replaceChildren();
    const video = make("video");
    video.setAttribute("controls", "");
    video.setAttribute("preload", "metadata");
    video.playsInline = true;
    const source = make("source");
    source.src = videoFile;
    source.type = videoFile.toLowerCase().endsWith(".webm")
      ? "video/webm"
      : "video/mp4";
    video.append(source);
    video.addEventListener("error", () => {
      mount.replaceChildren(
        make(
          "p",
          "",
          "No se pudo cargar el video configurado. Revisa la ruta en config.js.",
        ),
      );
    });
    mount.append(video);
  }
  /* Navegación por pestañas accesible */
  const tabButtons = [...document.querySelectorAll("[data-tab]")];
  const tabIds = {
    demo: "demoPanel",
    video: "videoPanel",
    circuit: "circuitPanel",
  };
  tabButtons.forEach((b, i) => {
    b.addEventListener("click", () => {
      tabButtons.forEach((t) => {
        const active = t === b;
        t.classList.toggle("selected", active);
        t.setAttribute("aria-selected", String(active));
        $(tabIds[t.dataset.tab]).hidden = !active;
      });
    });
    b.addEventListener("keydown", (e) => {
      if (e.key === "ArrowRight" || e.key === "ArrowLeft") {
        e.preventDefault();
        const next =
          (i + (e.key === "ArrowRight" ? 1 : -1) + tabButtons.length) %
          tabButtons.length;
        tabButtons[next].focus();
        tabButtons[next].click();
      }
    });
  });

  /* Simulación visual DIDÁCTICA, no conectada al dispositivo */
  let demoEntries = 0;
  let demoTimer = null;
  function demoStatus(msg) {
    text($("demoState"), msg);
    text($("demoLast"), msg.replace(" · ", " / "));
  }
  function demoOpen(reason, isPerson) {
    if (demoTimer) clearTimeout(demoTimer);
    $("virtualDoor").classList.add("open");
    $("demoRed").classList.remove("on");
    $("demoGreen").classList.add("on");
    if (isPerson) {
      demoEntries++;
      text($("demoEntries"), demoEntries);
    }
    demoStatus("Puerta abierta · " + reason);
    demoTimer = setTimeout(() => demoClose(true), 4200);
  }
  function demoClose(auto = false) {
    if (demoTimer) clearTimeout(demoTimer);
    demoTimer = null;
    $("virtualDoor").classList.remove("open");
    $("demoRed").classList.add("on");
    $("demoGreen").classList.remove("on");
    demoStatus(
      auto
        ? "Puerta cerrada · retorno automático"
        : "Puerta cerrada · en espera",
    );
  }
  $("detectBtn").addEventListener("click", () =>
    demoOpen("sensor simulado", true),
  );
  $("voiceBtn").addEventListener("click", () =>
    demoOpen("orden de voz simulada", false),
  );
  $("buttonBtn").addEventListener("click", () =>
    demoOpen("pulsador simulado", false),
  );
  $("resetBtn").addEventListener("click", () => {
    demoEntries = 0;
    text($("demoEntries"), "0");
    demoClose();
  });
  const SpeechRecognition =
    window.SpeechRecognition || window.webkitSpeechRecognition;
  const listen = $("listenBtn");
  if (!SpeechRecognition) {
    listen.disabled = true;
    listen.textContent = "Reconocimiento de voz no disponible";
  } else
    listen.addEventListener("click", () => {
      const rec = new SpeechRecognition();
      rec.lang = "es-EC";
      rec.interimResults = false;
      rec.maxAlternatives = 1;
      listen.disabled = true;
      listen.textContent = "Escuchando…";
      rec.onresult = (event) => {
        const phrase = event.results[0][0].transcript.toLowerCase();
        if (phrase.includes("abrir") && phrase.includes("puerta")) {
          demoOpen("comando de voz reconocido", false);
          text(
            $("voiceDemoHint"),
            "Reconocido: " + phrase + " (solo simulación virtual)",
          );
        } else
          text(
            $("voiceDemoHint"),
            "Escuchado: " + phrase + ". Prueba diciendo «abrir puerta».",
          );
      };
      rec.onerror = (ev) =>
        text($("voiceDemoHint"), "No se pudo escuchar: " + ev.error);
      rec.onend = () => {
        listen.disabled = false;
        listen.textContent = "Probar micrófono en esta simulación";
      };
      try {
        rec.start();
      } catch {
        listen.disabled = false;
      }
    });

  /* Telemetría REAL: no usar DEMO para rellenar datos. */
  const live = {
    source: null,
    lastAt: null,
    presence: null,
    door: null,
    entries: null,
    events: [],
    port: null,
    reader: null,
    wantStop: false,
    remoteMode: false,
  };
  const staleAfterMs = 45000;
  const sanitizeEvent = (v) =>
    String(v ?? "")
      .slice(0, 130)
      .replaceAll("_", " ");
  const formatTime = (ms, withDate = false) => {
    if (!Number.isFinite(ms)) return "—";
    return new Date(ms).toLocaleString(
      "es-EC",
      withDate
        ? { dateStyle: "short", timeStyle: "medium" }
        : { timeStyle: "medium" },
    );
  };
  const numericMs = (v) => {
    const n = Number(v);
    return Number.isFinite(n) && n > 0 ? n : null;
  };
  function showConnection() {
    const connected = !!live.lastAt && Date.now() - live.lastAt < staleAfterMs;
    const badge = $("liveBadge");
    badge.className =
      "status-tag " + (connected ? "connected" : "disconnected");
    badge.innerHTML =
      "<i></i> " +
      (connected
        ? "Conectado · datos reales"
        : "Sin conexión / sin datos recientes");
    if (connected)
      text(
        $("monitorConnectionHint"),
        "Origen: " +
          (live.source === "USB"
            ? "Puerto USB del ESP32"
            : "Firebase (datos enviados por el dispositivo)"),
      );
    else if (live.lastAt)
      text(
        $("monitorConnectionHint"),
        "Última lectura recibida; dispositivo sin datos recientes.",
      );
    else
      text(
        $("monitorConnectionHint"),
        "Esperando conexión con el dispositivo real.",
      );
  }
  function renderLive() {
    text(
      $("realPresence"),
      live.presence === null
        ? "—"
        : live.presence
          ? "Detectado"
          : "Sin presencia",
    );
    text(
      $("realDoor"),
      live.door === null
        ? "—"
        : live.door === "open"
          ? "Abierta"
          : live.door === "closed"
            ? "Cerrada"
            : String(live.door),
    );
    text($("realEntries"), live.entries === null ? "—" : live.entries);
    text($("realUpdated"), formatTime(live.lastAt));
    text(
      $("realUpdatedDate"),
      live.lastAt ? formatTime(live.lastAt, true) : "Sin lecturas todavía",
    );
    text($("historyCount"), live.events.length + " registros");
    const body = $("realHistory");
    body.replaceChildren();
    if (!live.events.length) {
      const tr = make("tr");
      const td = make(
        "td",
        "empty-table",
        "No se han recibido eventos del prototipo.",
      );
      td.colSpan = 3;
      tr.append(td);
      body.append(tr);
    } else
      live.events.slice(0, 30).forEach((ev) => {
        const tr = make("tr");
        tr.append(
          make("td", "", formatTime(ev.at)),
          make("td", "", sanitizeEvent(ev.name)),
          make("td", "", ev.source || "ESP32"),
        );
        body.append(tr);
      });
    showConnection();
  }
  function pushEvent(ev) {
    if (!ev?.name) return;
    if (live.events.some((e) => e.key === ev.key)) return;
    live.events.unshift(ev);
    live.events.sort((a, b) => b.at - a.at);
    live.events = live.events.slice(0, 50);
  }
  function applyReading(data, source) {
    if (!data || typeof data !== "object" || Array.isArray(data)) return;
    const received =
      source === "Firebase"
        ? numericMs(data.received_at_ms) || numericMs(data.timestamp_ms)
        : Date.now();
    if (!received) return;
    live.source = source;
    live.lastAt = received;
    if (typeof data.presence === "boolean") live.presence = data.presence;
    if (typeof data.presence === "number")
      live.presence = Boolean(data.presence);
    const d = String(data.door ?? "").toLowerCase();
    if (["open", "opened", "abierta", "abierto", "1"].includes(d))
      live.door = "open";
    else if (["closed", "cerrada", "cerrado", "0"].includes(d))
      live.door = "closed";
    if (
      Number.isInteger(Number(data.entries)) &&
      data.entries !== undefined &&
      data.entries !== null &&
      Number(data.entries) >= 0
    )
      live.entries = Number(data.entries);
    if (data.event && String(data.event).trim())
      pushEvent({
        key: source + "-" + received + "-" + String(data.event),
        at: received,
        name: String(data.event),
        source,
      });
    renderLive();
  }
  renderLive();
  setInterval(showConnection, 5000);
  const connectButton = $("serialBtn");
  if (!("serial" in navigator)) {
    connectButton.disabled = true;
    connectButton.textContent = "USB requiere Chrome/Edge en computadora";
  } else
    connectButton.addEventListener("click", async () => {
      if (live.port) {
        live.wantStop = true;
        try {
          if (live.reader) await live.reader.cancel();
        } catch {}
        return;
      }
      try {
        // El navegador solicita permiso explícito para acceder al dispositivo.
        const port = await navigator.serial.requestPort();
        await port.open({ baudRate: 115200 });
        live.port = port;
        live.wantStop = false;
        connectButton.textContent = "Desconectar USB";
        text(
          $("monitorGuide"),
          "Puerto abierto: esperando mensajes JSON verdaderos del ESP32. Usa solo una aplicación para controlar el mismo puerto USB.",
        );
        const reader = port.readable.getReader();
        live.reader = reader;
        const decoder = new TextDecoder();
        let buffer = "";
        try {
          while (true) {
            const r = await reader.read();
            if (r.done) break;
            buffer += decoder.decode(r.value, { stream: true });
            if (buffer.length > 10000) buffer = buffer.slice(-6000);
            let i;
            while ((i = buffer.indexOf("\n")) !== -1) {
              const line = buffer.slice(0, i).trim();
              buffer = buffer.slice(i + 1);
              if (!line) continue;
              try {
                applyReading(JSON.parse(line), "USB");
              } catch {
                /* Los mensajes no JSON se ignoran. */
              }
            }
          }
        } finally {
          reader.releaseLock();
          live.reader = null;
          try {
            await port.close();
          } catch {}
          live.port = null;
          live.lastAt = null;
          showConnection();
          connectButton.textContent = "Conectar ESP32 por USB";
        }
      } catch (err) {
        text(
          $("monitorGuide"),
          "Conexión USB no establecida: " +
            (err?.message || "sin permiso o puerto ocupado") +
            ". Cierra el Monitor Serial si lo está usando.",
        );
        if (live.port) {
          try {
            await live.port.close();
          } catch {}
          live.port = null;
          connectButton.textContent = "Conectar ESP32 por USB";
        }
      }
    });
  // Alternativa para público remoto: lectura de Firebase RTDB del puente Python.
  const firebaseUrl = safeLink(cfg.firebaseDatabaseUrl || "");
  if (firebaseUrl) {
    const base = firebaseUrl.replace(/\/$/, "");
    let lastRemoteKey = "";
    const poll = async () => {
      // No sustituimos datos anteriores con valores falsos si falla la red.
      if (live.port) return;
      try {
        const response = await fetch(base + "/neki/telemetry.json", {
          cache: "no-store",
        });
        if (!response.ok) throw new Error("HTTP " + response.status);
        const data = await response.json();
        if (data && typeof data === "object") {
          applyReading(data, "Firebase");
          const hr = await fetch(base + "/neki/history.json", {
            cache: "no-store",
          });
          if (hr.ok) {
            const history = await hr.json();
            if (history && typeof history === "object") {
              for (const [key, event] of Object.entries(history)) {
                const at = numericMs(event?.received_at_ms);
                if (at && event?.event)
                  pushEvent({
                    key: "firebase-" + key,
                    at,
                    name: String(event.event),
                    source: "Firebase",
                  });
              }
              renderLive();
            }
          }
        }
      } catch (e) {
        text(
          $("monitorGuide"),
          "No se pudieron consultar los datos públicos de Firebase: " +
            e.message +
            ". Revisa la URL y las reglas de solo lectura.",
        );
      }
    };
    poll();
    setInterval(poll, 4000);
  }

  /* CONTACTO REAL - ENVÍO A CORREO CON FORMSPREE */

  const contactForm = $("contactForm");

  contactForm.addEventListener("submit", async (e) => {
    e.preventDefault();

    const feedback = $("contactFeedback");
    const button = contactForm.querySelector('button[type="submit"]');

    const endpoint = cfg.formspreeEndpoint;

    if (!endpoint || !endpoint.startsWith("https://formspree.io/f/")) {
      text(feedback, "Configura primero Formspree.");
      return;
    }

    if (!contactForm.reportValidity()) return;

    const buttonText = button.innerHTML;

    button.disabled = true;
    button.textContent = "Enviando mensaje...";
    text(feedback, "Enviando tu consulta...");

    try {
      const response = await fetch(endpoint, {
        method: "POST",
        body: new FormData(contactForm),
        headers: {
          Accept: "application/json",
        },
      });

      if (!response.ok) {
        throw new Error("Error al enviar");
      }

      text(
        feedback,
        "¡Mensaje enviado correctamente! Gracias por contactar a NEKI Hub.",
      );

      contactForm.reset();
    } catch (error) {
      text(feedback, "No se pudo enviar el mensaje. Intenta nuevamente.");
    } finally {
      button.disabled = false;
      button.innerHTML = buttonText;
    }
  });
})();
