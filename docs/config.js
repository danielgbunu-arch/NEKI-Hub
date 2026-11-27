/* NEKI: edita este archivo para personalizar la web sin modificar el HTML. */
window.NEKI_CONFIG = {
  company: "NEKI Technologies",
  product: "NEKI Hub",
  tagline: "Tecnología inteligente para proteger tu negocio",
  contactEmail: "danielgbunu@gmail.com", // Dirección de ejemplo; sustituir antes de publicar

  formspreeEndpoint: "https://formspree.io/f/mgakqqdv", // Endpoint de Formspree para el formulario de contacto

  websiteUrl: "", // URL pública, cuando se publique
  githubUrl: "https://github.com/danielgbunu-arch/NEKI-Hub", // Repositorio público del equipo
  simulationUrl: "https://wokwi.com/projects/481816355416515585", // Enlace público del circuito Wokwi/Tinkercad

  socialLinks: [
    {
      label: "GitHub",
      url: "https://github.com/danielgbunu-arch/NEKI-Hub",
    },
    {
      label: "Simulación Wokwi",
      url: "https://wokwi.com/projects/481816355416515585",
    },
  ],

  media: {
    prototypePhoto: "", // p. ej. 'assets/prototipo-real.jpg'
    gallery: [
      {
        title: "Componentes utilizados",
        subtitle:
          "ESP32, sensor ultrasónico HC-SR04, pantalla LCD, indicadores LED y conexiones eléctricas.",
        src: "assets/img3.png",
      },
      {
        title: "Montaje del circuito",
        subtitle:
          "Integración del controlador L298N y los componentes electrónicos del sistema NEKI Hub.",
        src: "assets/img1.jpeg",
      },

      {
        title: "Pruebas del prototipo",
        subtitle:
          "Demostración real del funcionamiento del circuito electrónico NEKI Hub.",
        src: "",
        video: "assets/prueba.mp4",
      },

      {
        title: "Maqueta terminada",
        subtitle: "Fotografía real del resultado",
        src: "",
      },
    ],
    demoVideo: "assets/demo.mp4", // p. ej. 'assets/neki-demo.mp4' (indicar que es video conceptual IA)
  },
  // Se puede usar USB directamente en una laptop Chrome/Edge, sin configurar Firebase.
  // Para monitorear desde celulares también, configurar Firebase Realtime Database.
  firebaseDatabaseUrl: "", // Ej.: 'https://mi-proyecto-default-rtdb.firebaseio.com'
  team: [
    {
      name: "Daniel Guzmán",
      role: "Lider de Grupo",
      photo: "assets/int4.jpeg",
    },
    {
      name: "Paula Guzmán",
      role: "Sublider de Grupo",
      photo: "assets/int3.jpeg",
    },
    {
      name: "sebastian Jara",
      role: "Integrante Hardware",
      photo: "assets/int1.jpeg",
    },
    {
      name: "Steven Cando",
      role: "Integrante Software",
      photo: "assets/int2.jpeg",
    },
    {
      name: "Miguel Zambrano",
      role: "Integrante Software",
      photo: "assets/int5.jpeg",
    },
    {
      name: "Juan Montalvan",
      role: "Integrante Hardware",
      photo: "assets/int6.jpeg",
    },
    {
      name: "Miguel Vaca",
      role: "Integrante Hardware",
      photo: "assets/int7.jpeg",
    },
    {
      name: "Alex Auter",
      role: "Integrante Software",
      photo: "assets/int8.jpeg",
    },
  ],
};
