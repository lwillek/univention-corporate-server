/*
 * Copyright 2011-2012 Univention GmbH
 *
 * http://www.univention.de/
 *
 * All rights reserved.
 *
 * The source code of this program is made available
 * under the terms of the GNU Affero General Public License version 3
 * (GNU AGPL V3) as published by the Free Software Foundation.
 *
 * Binary versions of this program provided by Univention to you as
 * well as other copyrighted, protected or trademarked materials like
 * Logos, graphics, fonts, specific documentations and configurations,
 * cryptographic keys etc. are subject to a license agreement between
 * you and Univention and not subject to the GNU AGPL V3.
 *
 * In the case you use this program under the terms of the GNU AGPL V3,
 * the program is provided in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public
 * License with the Debian GNU/Linux or Univention distribution in file
 * /usr/share/common-licenses/AGPL-3; if not, see
 * <http://www.gnu.org/licenses/>.
 */
/*global define */

define([
	"dojo/_base/lang",
	"dojo/date/locale",
	"umc/dialog",
	"umc/i18n!umc/app"
], function(lang, locale, dialog, _) {
	var about = {};
	lang.mixin(about, {
		show: function( info ) {
			var keys = {
				labelServer : _( 'Server' ),
				server : info.server,
				labelUCS_Version : _( 'UCS version' ),
				UCS_Version : info.ucs_version,
				labelUMC_Version : _( 'UMC version' ),
				UMC_Version : info.umc_version,
				labelSSL_ValidityDate : _( 'Validity date of the SSL certificate' ),
				SSL_ValidityDate : locale.format(new Date(info.ssl_validity_date), {
					fullYear: true,
					timePattern: " ",
					formatLength: "long"
				})
			};
			dialog.templateDialog( "umc", "about.html", keys, _( 'About UMC' ), _( 'Close' ) );
		}
	});
	return about;
} );
