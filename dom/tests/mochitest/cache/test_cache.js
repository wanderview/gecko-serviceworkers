ok(!!caches, 'caches object should be available on global');
caches.create('snafu').then(function(cache) {
  ok(!!cache, 'cache object should be resolved from caches.create');
  workerTestDone();
});
