DROP TABLE IF EXISTS `dns_domains`;
CREATE TABLE `dns_domains` (
  `id` int unsigned NOT NULL auto_increment,
  `moniker_rec_id` char(36) NOT NULL DEFAULT '',
  `tenant_id` char(36) DEFAULT NULL,
  `domain_id` char(36) NOT NULL DEFAULT '',
  `name` varchar(255) DEFAULT NULL,
  `ttl` int(11) DEFAULT NULL,
  `type` enum('A', 'AAAA', 'AFSDB', 'APL', 'CERT', 'CNAME',
              'DHCID', 'DLV', 'DNAME', 'DNSKEY', 'DS', 'HIP',
              'IPSECKEY', 'KEY', 'KX', 'LOC', 'MX', 'NAPTR',
              'NS', 'NSEC', 'NSEC3', 'NSEC3PARAM', 'PTR', 'RRSIG',
              'RP', 'SIG', 'SOA', 'SPF', 'SRV', 'SSHFP', 'TA', 'TKEY',
              'TLSA', 'TSIG', 'TXT', 'AXFR', 'IXFR', 'OPT' ) DEFAULT NULL,
  `data` varchar(255) DEFAULT NULL,
  PRIMARY KEY (id, tenant_id),
  KEY (tenant_id, domain_id, name(36)),
  KEY (moniker_rec_id)
)
ENGINE=InnoDB DEFAULT CHARSET utf8 
PARTITION BY KEY(tenant_id) PARTITIONS 100;

