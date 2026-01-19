import React from 'react';
import { useTranslation } from 'react-i18next';
import { FormCheck } from 'react-bootstrap';
import * as yup from 'yup';

import Section from '../Components/Section';
import { AddonPropTypes } from '../Pages/AddonsConfigPage';

// Схема валидации (простая, так как только вкл/выкл)
export const dualPicoHostScheme = {
	DualPicoHostAddonEnabled: yup
		.number()
		.label('Dual Pico Host Add-On Enabled'),
};

// Начальное состояние
export const dualPicoHostState = {
	DualPicoHostAddonEnabled: 0,
};

const DualPicoHost = ({
	values,
	handleCheckbox,
}: AddonPropTypes) => {
	const { t } = useTranslation();

	return (
		<Section title={t('AddonsConfig:dual-pico-host-header-text')}>
			<div className="row mb-3">
				<p>{t('AddonsConfig:dual-pico-host-desc-text')}</p>
				<FormCheck
					label={t('Common:switch-enabled')}
					type="switch"
					id="DualPicoHostAddonButton"
					reverse
					isInvalid={false}
					checked={Boolean(values.DualPicoHostAddonEnabled)}
					onChange={(e) => {
						handleCheckbox('DualPicoHostAddonEnabled');
					}}
				/>
			</div>
            {/* Если нужно добавить предупреждение про UART */}
            {Boolean(values.DualPicoHostAddonEnabled) && (
                <div className="alert alert-info">
                    {t('AddonsConfig:dual-pico-host-uart-notice')}
                </div>
            )}
		</Section>
	);
};

export default DualPicoHost;