function time_format(time) {
    var date = new Date(time * 1000);
    return date.toLocaleString();
}

// Change unix time to human time
for (var element of document.getElementsByClassName("time")) {
    element.innerHTML = time_format(element.innerHTML);
}

// Init map
var map = L.map('map').setView([10.82302, 106.62965], 0);

// Select map tile layer as OSM
L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png', {
    detectRetina: true,
    maxZoom: 20,
    attribution: '&copy; <a href="http://www.openstreetmap.org/copyright">OpenStreetMap</a>'
}).addTo(map);

var marker;
function change_location(index) {
    var lat = crash_history[index].lat;
    var lng = crash_history[index].lng;

    map.setView([lat, lng], 15);
    if (marker && map.hasLayer(marker)) {
        map.removeLayer(marker); // remove
    }
    marker = L.marker([lat, lng]).addTo(map);
    marker.bindPopup("<b>" + crash_history[index].lat + ", " + crash_history[index].lng + "</b><br>" + time_format(crash_history[index].time)).openPopup();
}

// Use the latest data to set view
change_location(0);

function show_google_maps() {
    lat_lng = marker.getLatLng();
    url = `https://www.google.com/maps/search/?api=1&query=${lat_lng.lat},${lat_lng.lng}`;
    window.open(url, '_blank');
}


//---------------- Notification ----------------//


// Check for push notification support
if (!('serviceWorker' in navigator) || !('PushManager' in window)) {
    let span = document.getElementById("notification_span")
    let btn = document.getElementById("notification_button")
    btn.classList.add("disabled");
    span.setAttribute("data-bs-title", "Your browser does not support push notification");
}

// Enable tooltips
const tooltipTriggerList = document.querySelectorAll('[data-bs-toggle="tooltip"]')
const tooltipList = [...tooltipTriggerList].map(tooltipTriggerEl => new bootstrap.Tooltip(tooltipTriggerEl))

// Service worker
function registerServiceWorker() {
    return navigator.serviceWorker
        .register(service_worker_url)
        .then(function (registration) {
            console.log('Service worker successfully registered.');
            return registration;
        })
        .catch(function (err) {
            console.error('Unable to register service worker.', err);
        });
}

// Notification permission
function askPermission() {
    return new Promise(function (resolve, reject) {
        const permissionResult = Notification.requestPermission(function (result) {
            resolve(result);
        });

        if (permissionResult) {
            permissionResult.then(resolve, reject);
        }
    }).then(function (permissionResult) {
        if (permissionResult !== 'granted') {
            throw new Error("We weren't granted permission.");
        }
    });
}

// Subscribe to push notification
function subscribeUserToPush() {
    return navigator.serviceWorker
        .register(service_worker_url)
        .then(function (registration) {
            const subscribeOptions = {
                userVisibleOnly: true,
                applicationServerKey: public_key,
            };

            return registration.pushManager.getSubscription().then(function (existedSubscription) {
                if (existedSubscription === null) {
                    return registration.pushManager.subscribe(subscribeOptions);
                } else {
                    return existedSubscription;
                }
            });
        })
        .then(function (pushSubscription) {
            console.log(
                'Received PushSubscription: ',
                JSON.stringify(pushSubscription),
            );
            return pushSubscription;
        });
}

function sendSubscriptionToBackEnd(subscription) {
    return fetch(save_sub_url, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify(subscription),
    })
        .then(function (response) {
            if (!response.ok) {
                throw new Error('Bad status code from server.');
            }

            return response.json();
        })
        .then(function (responseData) {
            if (!(responseData.data)) {
                throw new Error('Bad response from server.');
            }
        });
}


function enable_notification() {
    askPermission().then(function (result) {
        subscribeUserToPush().then(function (subscription) {
            sendSubscriptionToBackEnd(subscription);
        });
    });
}

function test_notification() {
    fetch("/test_notification");
}
