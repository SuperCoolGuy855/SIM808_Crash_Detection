self.addEventListener('push', function (event) {
    let data = event.data.json();
    let title = data["title"];
    let options = {
        body: data["message"],
    };
    event.waitUntil(self.registration.showNotification(title, options));
});

self.addEventListener('notificationclick', (event) => {
    const clickedNotification = event.notification;
    clickedNotification.close();

    const examplePage = '/';
    const promiseChain = clients.openWindow(examplePage);
    event.waitUntil(promiseChain);
});
